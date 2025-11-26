/*
 * Copyright (c) 2012-2025, Tiiffi <tiiffi at gmail>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software
 *   in a product, an acknowledgment in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be
 *   misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *   distribution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #include <fcntl.h>
    #include <wchar.h>
#else
    #include <stdio_ext.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <netdb.h>
#endif

#define VERSION "0.8.0"
#define IN_NAME "mcrcon"
#define VER_STR IN_NAME" "VERSION" (built: "__DATE__" "__TIME__")"

#define RCON_EXEC_COMMAND       2
#define RCON_AUTHENTICATE       3
#define RCON_RESPONSEVALUE      0
#define RCON_AUTH_RESPONSE      2
#define RCON_PID                0xBADC0DE

#define MAX_COMMAND_LENGTH      4096
#define DATA_BUFFSIZE           MAX_COMMAND_LENGTH + 2 // plus two null terminators
#define MAX_PACKET_SIZE         4106 // id (4) + cmd (4) + DATA_BUFFSIZE
#define MIN_PACKET_SIZE         10   // id (4) + cmd (4) + two empty strings (2)

#define MAX_WAIT_TIME           600

#define log_error(fmt, ...) fprintf(stderr, fmt,  ##__VA_ARGS__);

// rcon packet structure
typedef struct {
    int32_t size;
    int32_t id;
    int32_t cmd;
    uint8_t data[DATA_BUFFSIZE];
    // ignoring string2 for now
} rc_packet;

// ===================================
//  FUNCTION DEFINITIONS
// ===================================

// Network related functions
#ifdef _WIN32
void        net_init_WSA(void);
#endif
void        net_close(int sd);
int         net_connect(const char *host, const char *port);
bool        net_send_packet(int sd, rc_packet *packet);
rc_packet*  net_recv_packet(int sd);

// Misc stuff
void        usage(void);
void        set_color(int c);
int         get_line(char *buffer, int len);
int         run_terminal_mode(int sock);
int         run_commands(int argc, char *argv[]);

// Rcon protocol related functions
rc_packet*  packet_build(int id, int cmd, char s[static 1]);
void        packet_print(rc_packet *packet);
bool        rcon_auth(int sock, char *passwd);
int         rcon_command(int sock, char *command);

// =============================================
//  GLOBAL VARIABLES
// =============================================
static int  flag_raw_output = 0;
static int  flag_silent_mode = 0;
static int  flag_disable_colors = 0;
static int  flag_wait_seconds = 0;
static int  global_connection_alive = 1;
static bool global_valve_protocol = false;
static bool global_minecraft_newline_fix = false;
static int  global_rsock;

#ifdef _WIN32
// console coloring on windows
HANDLE console_output_handle;
HANDLE console_input_handle;

// console code pages
UINT old_output_codepage;
UINT old_input_codepage;
#endif

// safety stuff (windows is still misbehaving)
void exit_proc(void)
{
    if (global_rsock != -1)
        net_close(global_rsock);

    #ifdef _WIN32
    // Restore previous code pages
    SetConsoleOutputCP(old_output_codepage);
    SetConsoleCP(old_input_codepage);
    #endif
}

// TODO: check exact windows and linux behaviour
void sighandler(int sig)
{
    if (sig == SIGINT)
        putchar('\n');

    global_connection_alive = 0;
    exit(EXIT_SUCCESS);
}

unsigned int mcrcon_parse_seconds(char *str)
{
    char *end;
    long result = strtol(str, &end, 10);

    if (errno != 0) {
        log_error("-w invalid value.\nerror %d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (end == str) {
        log_error("-w invalid value (not a number?)\n");
        exit(EXIT_FAILURE);
    }

    if (result <= 0 || result > MAX_WAIT_TIME) {
        log_error("-w value out of range.\nAcceptable value is 1 - %d (seconds).\n", MAX_WAIT_TIME);
        exit(EXIT_FAILURE);
    }

    return (unsigned int) result;
}

int main(int argc, char *argv[])
{
    int terminal_mode = 0;

    char *host = getenv("MCRCON_HOST");
    char *pass = getenv("MCRCON_PASS");
    char *port = getenv("MCRCON_PORT");

    if (!port) port = "25575";
    if (!host) host = "localhost";

    if(argc < 1 && pass == NULL) usage();

    // default getopt error handler enabled
    opterr = 1;
    int opt;
    while ((opt = getopt(argc, argv, "vrtcshw:H:p:P:")) != -1)
    {
        switch (opt) {
            case 'H': host = optarg;                 break;
            case 'P': port = optarg;                 break;
            case 'p': pass = optarg;                 break;
            case 'c': flag_disable_colors = 1;       break;
            case 's': flag_silent_mode = 1;          break;
            case 'i': /* reserved for interp mode */ break;
            case 't': terminal_mode = 1;             break;
            case 'r': flag_raw_output = 1;           break;
            case 'w':
                flag_wait_seconds = mcrcon_parse_seconds(optarg);
                break;

            case 'v':
                puts(VER_STR);
                puts("Bug reports:\n\ttiiffi+mcrcon at gmail\n\thttps://github.com/Tiiffi/mcrcon/issues/");
                exit(EXIT_SUCCESS);

            case 'h': usage(); break;
            case '?':
            default:
                puts("Try 'mcrcon -h' or 'man mcrcon' for help.");
                exit(EXIT_FAILURE);
        }
    }

    if (pass == NULL) {
        puts("You must give password (-p password).\nTry 'mcrcon -h' or 'man mcrcon' for help.");
        exit(EXIT_FAILURE);
    }

    if(optind == argc && terminal_mode == 0) {
        terminal_mode = 1;
    }

    // safety features to prevent "IO: Connection reset" bug on the server side
    atexit(&exit_proc);
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);

    #ifdef _WIN32
    net_init_WSA();

    console_input_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (console_input_handle == INVALID_HANDLE_VALUE || console_input_handle == NULL) {
        log_error("Error: Failed to get console input handle.\n");
        exit(EXIT_FAILURE);
    }

    console_output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console_output_handle == INVALID_HANDLE_VALUE)
        console_output_handle = NULL;

    // Set the output and input code pages to utf-8
    old_output_codepage = GetConsoleOutputCP();
    old_input_codepage = GetConsoleCP();

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif

    // open socket
    global_rsock = net_connect(host, port);

    int exit_code = EXIT_SUCCESS;

    // auth & commands
    if (rcon_auth(global_rsock, pass)) {
        if (terminal_mode) exit_code = run_terminal_mode(global_rsock);
        else exit_code = run_commands(argc, argv);
    }
    else { // auth failed
        log_error("Authentication failed!\n");
        exit_code = EXIT_FAILURE;
    }

    exit(exit_code);
}

void usage(void)
{
    puts(
        "Usage: "IN_NAME" [OPTIONS] [COMMANDS]\n\n"
        "Send rcon commands to Minecraft server.\n\n"
        "Options:\n"
        "  -H\t\tServer address (default: localhost)\n"
        "  -P\t\tPort (default: 25575)\n"
        "  -p\t\tRcon password\n"
        "  -t\t\tTerminal mode\n"
        "  -s\t\tSilent mode\n"
        "  -c\t\tDisable colors\n"
        "  -r\t\tOutput raw packets\n"
        "  -w\t\tWait for specified duration (seconds) between each command (1 - 600s)\n"
        "  -h\t\tPrint usage\n"
        "  -v\t\tVersion information\n\n"
        "Server address, port and password can be set with following environment variables:\n"
        "  MCRCON_HOST\n"
        "  MCRCON_PORT\n"
        "  MCRCON_PASS\n"
    );

    puts (
        "- mcrcon will start in terminal mode if no commands are given\n"
        "- Command-line options will override environment variables\n"
        "- Rcon commands with spaces must be enclosed in quotes\n"
    );
    puts("Example:\n\t"IN_NAME" -H my.minecraft.server -p password -w 5 \"say Server is restarting!\" save-all stop\n");

    #ifdef _WIN32
    puts("Press enter to exit.");
    getchar();
    #endif

    exit(EXIT_SUCCESS);
}

#ifdef _WIN32
void net_init_WSA(void)
{
    WSADATA wsadata;

    // Request winsock 2.2 for now.
    // Should be compatible down to Win XP.
    WORD version = MAKEWORD(2, 2);

    int err = WSAStartup(version, &wsadata);
    if (err != 0) {
        log_error("WSAStartup failed. Error: %d.\n", err);
        exit(EXIT_FAILURE);
    }
}
#endif

// socket close and cleanup
void net_close(int sd)
{
    #ifdef _WIN32
    closesocket(sd);
    WSACleanup();
    #else
    close(sd);
    #endif
}

// http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
// https://bugs.chromium.org/p/chromium/issues/detail?id=44489
int net_connect(const char *host, const char *port)
{
    int sd;

    struct addrinfo hints;
    struct addrinfo *server_info, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    #ifdef _WIN32
    net_init_WSA();
    #endif

    int ret = getaddrinfo(host, port, &hints, &server_info);
    if (ret != 0) {
        log_error("Name resolution failed.\n");
        #ifdef _WIN32
        log_error("Error %d: %s", ret, gai_strerror(ret));
        #else
        log_error("Error %d: %s\n", ret, gai_strerror(ret));
        #endif

        exit(EXIT_FAILURE);
    }

    // Go through the hosts and try to connect
    for (p = server_info; p != NULL; p = p->ai_next) {
        sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sd == -1)
            continue;

        ret = connect(sd, p->ai_addr, p->ai_addrlen);
        if (ret == -1) {
            net_close(sd);
            continue;
        }
        // Get out of the loop when connect is successful
        break;
    }

    if (p == NULL) {
        /* TODO (Tiiffi): Check why windows does not report errors */
        log_error("Connection failed.\n");
        #ifndef _WIN32
        log_error("Error %d: %s\n", errno, strerror(errno));
        #endif

        freeaddrinfo(server_info);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);
    return sd;
}

bool net_send_packet(int sd, rc_packet *packet)
{
    size_t sent = 0;
    size_t size = packet->size + sizeof(int32_t);
    size_t left = size;

    char *p = (char *) packet;

    while (sent < size) {
        ssize_t result = send(sd, p + sent, left, 0);

        if (result == -1) return false;

        sent += result;
        left -= sent;
    }

    return true;
}


// TODO: Fix the wonky behaviour when server quit/exit/stop/close
//       command is issued. Client should close gracefully without errors.
rc_packet *net_recv_packet(int sd)
{
    int32_t psize;
    static rc_packet packet = {0};

    ssize_t ret = recv(sd, (char *) &psize, sizeof(psize), 0);

    if (ret == 0) {
        log_error("Connection lost.\n");
        global_connection_alive = 0;
        return NULL;
    }

    if (ret != sizeof(psize)) {
        log_error("Error: recv() failed.\n");
        global_connection_alive = 0;
        return NULL;
    }

    if (psize < MIN_PACKET_SIZE || psize > MAX_PACKET_SIZE) {
        log_error("Error: Invalid packet size (%d).\n", psize);
        global_connection_alive = 0;
        return NULL;
    }

    packet.size = psize;
    char *p = (char *) &packet;

    int received = 0;
    while (received < psize) {
        ret = recv(sd, p + sizeof(int32_t) + received, psize - received, 0);
        if (ret == 0) {
            log_error("Connection lost.\n");
            global_connection_alive = 0;
            return NULL;
        }

        received += ret;
    }

    return &packet;
}

/* NOTE: At least Bukkit/Spigot servers are sending formatting via rcon but not sure how common this
 * behaviour is. This implementation tries to print colors but is omitting "bold" in "bright" colors
 * because bold tag should be manually reset and Bukkit/Spigot is not doing this.
 * References: https://minecraft.fandom.com/wiki/Formatting_codes
 *             https://en.wikipedia.org/wiki/ANSI_escape_code
 */
void set_color(int c)
{
    c = tolower(c);

    #ifdef _WIN32
    // Map color Minecraft color codes to WinAPI colors
    if (c >= '0' && c <= '9') {
        c -= '0';
    }
    else if (c >= 'a' && c <= 'f') {
        c -= 87;
    }
    else return;

    SetConsoleTextAttribute(console_output_handle, c);
    #else

    const char *ansi_escape;
    switch (c)
    {
        case '0': ansi_escape = "\033[30m";   break; // Black
        case '1': ansi_escape = "\033[34m";   break; // Blue
        case '2': ansi_escape = "\033[32m";   break; // Green
        case '3': ansi_escape = "\033[36m";   break; // Cyan
        case '4': ansi_escape = "\033[31m";   break; // Red
        case '5': ansi_escape = "\033[35m";   break; // Magenta
        case '6': ansi_escape = "\033[33m";   break; // Yellow
        case '7': ansi_escape = "\033[37m";   break; // White
        case '8': ansi_escape = "\033[1;90m"; break; // Bright black (gray)
        case '9': ansi_escape = "\033[1;94m"; break; // Bright blue
        case 'a': ansi_escape = "\033[1;92m"; break; // Bright green
        case 'b': ansi_escape = "\033[1;96m"; break; // Bright cyan
        case 'c': ansi_escape = "\033[1;91m"; break; // Bright red
        case 'd': ansi_escape = "\033[1;95m"; break; // Bright magenta
        case 'e': ansi_escape = "\033[1;93m"; break; // Bright yellow
        case 'f': ansi_escape = "\033[1;97m"; break; // Bright white
        case 'k': ansi_escape = "\033[08m";   break; // Concealed text
        case 'l': ansi_escape = "\033[01m";   break; // Bold text
        case 'm': ansi_escape = "\033[09m";   break; // Strikethrough text
        case 'n': ansi_escape = "\033[03m";   break; // Italic text
        case 'o': ansi_escape = "\033[04m";   break; // Underlined text
        case  0: // fallthrough
        case 'r': ansi_escape = "\033[00m";   break; // Reset all attributes
        default: return;

    }
    fputs(ansi_escape, stdout);
    #endif
}

// this hacky mess might use some optimizing
void packet_print(rc_packet *packet)
{
    uint8_t *data = packet->data;
    int i;

    if (flag_raw_output == 1 || global_valve_protocol == true) {
        fputs((char *) data, stdout);
        return;
    }

    // Newline fix for Minecraft
    if (global_valve_protocol == false) {
        const char test[] = "Unknown or incomplete command, see below for error";
        size_t test_size = sizeof test - 1;
        if (strncmp((char *) data, test, test_size) == 0) {
            fwrite(data, test_size, 1, stdout);
            putchar('\n');
            data = &data[test_size];
        }
    }

    int default_color = 'r';

    #ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO console_info;
    if (GetConsoleScreenBufferInfo(console_output_handle, &console_info) != 0) {
        default_color = console_info.wAttributes + 0x30;
    }
    else default_color = 0x37;
    #endif

    bool slash = false;
    bool colors_detected = false;

    for (i = 0; data[i] != 0; ++i)
    {
        // Check for '§' (utf-8 section sign). Bukkit is using this for colors.
        if(data[i] == 0xc2 && data[i + 1] == 0xa7) {
            // Disable new line fixes if Bukkit colors are detected
            colors_detected = true;
            i += 2;
            if (flag_disable_colors == 0) {
                set_color(default_color);
                set_color(data[i]);
            }
            continue;
        }

        // Add missing newlines
        if (colors_detected == false && global_minecraft_newline_fix && data[i] == '/') {
            if (data[i - 1] != '\n')
                slash ? putchar('\n') : (slash = true);
        }

        putchar(data[i]);
    }

    set_color(default_color); // reset color
    fflush(stdout);
}

rc_packet *packet_build(int id, int cmd, char s[static 1])
{
    static rc_packet packet = {0};

    // NOTE(Tiiffi): Issue report states that maximum command packet size is 1460 bytes:
    //               https://github.com/Tiiffi/mcrcon/issues/45#issuecomment-1000940814
    //               https://mctools.readthedocs.io/en/master/rcon.html
    //               Have to do some testing to confirm!

    int len = strlen(s);
    if (len > MAX_COMMAND_LENGTH) {
        log_error("Warning: Command string too long (%d). Maximum allowed: %d.\n", len, MAX_COMMAND_LENGTH);
        return NULL;
    }

    packet.size = sizeof packet.id + sizeof packet.cmd + len + 2;
    packet.id = id;
    packet.cmd = cmd;
    if (packet.size > 0) memcpy(packet.data, s, len);
    packet.data[len] = 0;
    packet.data[len + 1] = 0;

    return &packet;
}

bool rcon_auth(int sock, char *passwd)
{
    rc_packet *packet = packet_build(RCON_PID, RCON_AUTHENTICATE, passwd);
    if (packet == NULL)
        return 0;

    if (!net_send_packet(sock, packet)) {
        return 0; // send failed
    }

    receive:
    packet = net_recv_packet(sock);
    if (packet == NULL)
        return 0;

    /* Valve rcon sends empty "RCON_RESPONSEVALUE" packet before real auth response
     * so we have to check packet type and try again if necessary.
     */
    if (packet->cmd != RCON_AUTH_RESPONSE) {
        global_valve_protocol = true;
        goto receive;
    }

    // return true if authentication OK
    return packet->id == -1 ? false : true;
}

int rcon_command(int sock, char *command)
{
    if (global_valve_protocol == false && strcasecmp(command, "help") == 0)
        global_minecraft_newline_fix = true;

    rc_packet *packet = packet_build(RCON_PID, RCON_EXEC_COMMAND, command);
    if (packet == NULL) {
        log_error("Error: packet build() failed!\n");
        return 0;
    }

    if (!net_send_packet(sock, packet)) {
        log_error("Error: net_send_packet() failed!\n");
        return 0;
    }

    // workaround to handle valve style multipacket responses
    packet = packet_build(0xBADA55, 0xBADA55, "");
    if (packet == NULL) {
        log_error("Error: packet build() failed!\n");
        return 0;
    }

    if (!net_send_packet(sock, packet)) {
        log_error("Error: net_send_packet() failed!\n");
        return 0;
    }

    // initialize stuff for select()
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    // Set 5 second timeout
    struct timeval timeout = {0};
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    char last_character = '\0';
    int incoming = 0;

    do {
        packet = net_recv_packet(sock);
        if (packet == NULL) {
            log_error("Error: net_recv_packet() failed!\n");
            return 0;
        }

        // Check for packet id and multipacket guard id
        if (packet->id != RCON_PID && packet->id != 0xBADA55) {
            log_error("Error: invalid packet id!\n");
            return 0;
        }

        if (packet->id == 0xBADA55) {
            // Print newline after receiving multipacket guard packet
            if (last_character != '\n') {
                putchar('\n');
                fflush(stdout);
            }
            break;
        }

        if (flag_silent_mode == false) {
            if (packet->size > 10) {
                packet_print(packet);
                last_character = packet->data[packet->size - 11];
            }
        }

        int result = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        if (result == -1) {
            log_error("Error: select() failed!\n");
            return 0;
        }

        incoming = (result > 0 && FD_ISSET(sock, &read_fds));
    }
    while(incoming);

    return 1;
}

int run_commands(int argc, char *argv[])
{
    int i = optind;

    for (;;) {
        if (!rcon_command(global_rsock, argv[i]))
            return EXIT_FAILURE;

        i++;

        if (i >= argc)
            return EXIT_SUCCESS;

        if (flag_wait_seconds > 0) {
            #ifdef _WIN32
            Sleep(flag_wait_seconds * 1000);
            #else
            sleep(flag_wait_seconds);
            #endif
        }
    }
}

// terminal mode
int run_terminal_mode(int sock)
{
    char command[MAX_COMMAND_LENGTH] = {0};

    puts("Logged in. Press Ctrl-D or Ctrl-C to disconnect.");

    while (global_connection_alive) {
        putchar('>');
        fflush(stdout);

        int len = get_line(command, MAX_COMMAND_LENGTH);
        if (len < 1) continue;

        if (len > 0 && global_connection_alive) {
            if (!rcon_command(sock, command)) {
                return EXIT_FAILURE;
            }
        }

        /* Special case for "stop" command to prevent server-side bug.
         * https://bugs.mojang.com/browse/MC-154617
         *
         * NOTE: Not sure if this still a problem?!
         */
        if (global_valve_protocol == false && strcasecmp(command, "stop") == 0) {
            // Timeout to before checking if connection was closed
            #ifdef _WIN32
            Sleep(2 * 1000);
            #else
            sleep(2);
            #endif

            char tmp[1];
            #ifdef _WIN32
            // TODO: More Windows side testing!
            int result = recv(sock, tmp, sizeof(tmp), MSG_PEEK | 0);
            #else
            int result = recv(sock, tmp, sizeof(tmp), MSG_PEEK | MSG_DONTWAIT);
            #endif
            if (result == 0) {
                break; // Connection closed
            }
            // TODO: Check for return values and errors!
        }
    }

    return EXIT_SUCCESS;
}

#ifdef _WIN32
#define WCHAR_BUFFER_SIZE (MAX_COMMAND_LENGTH / 3)
char *windows_getline(char *buf, int size)
{
    WCHAR wide_buffer[WCHAR_BUFFER_SIZE];
    DWORD chars_read = 0;

    if (!ReadConsoleW(console_input_handle, wide_buffer, WCHAR_BUFFER_SIZE - 1, &chars_read, NULL))
        return NULL;

    // Check if we hit buffer limit (no newline = more data waiting)
    int has_newline = (chars_read > 0 && wide_buffer[chars_read - 1] == L'\n');
    if (!has_newline) {
        // Buffer was full, drain the rest until newline
        WCHAR drain_buffer[256];
        DWORD drain_read;
        do {
            if (!ReadConsoleW(console_input_handle, drain_buffer, 255, &drain_read, NULL))
                break;
        } while (drain_read > 0 && drain_buffer[drain_read - 1] != L'\n');
    }

    if (chars_read > 0 && wide_buffer[chars_read - 1] == L'\n') {
        chars_read--;
        if (chars_read > 0 && wide_buffer[chars_read - 1] == L'\r')
            chars_read--;
    }
    wide_buffer[chars_read] = L'\0';

    int bytes_needed = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1, NULL, 0, NULL, NULL);
    if (bytes_needed <= 0 || bytes_needed > size) {
        log_error("Widechar to UTF-8 conversion failed.\n");
        exit(EXIT_FAILURE);
    }

    int result = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1, buf, size, NULL, NULL);
    if (result == 0) {
        log_error("Widechar to UTF-8 conversion failed.\n");
        exit(EXIT_FAILURE);
    }

    return buf;
}
#endif

// gets line from stdin and deals with rubbish left in the input buffer
int get_line(char *buffer, int bsize)
{
    #ifdef _WIN32
    char *ret = windows_getline(buffer, bsize);
    #else
    char *ret = fgets(buffer, bsize, stdin);
    #endif

    if (ret == NULL) {
        if (ferror(stdin)) {
            log_error("Error %d: %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
        // EOF
        putchar('\n');
        exit(EXIT_SUCCESS);
    }

    // remove unwanted characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = '\0';
    int len = strlen(buffer);

    if (len == bsize - 1) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);
    }

    return len;
}
