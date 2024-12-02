# mcrcon

mcrcon is console based Minecraft [rcon](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol) client for remote administration and server maintenance scripts.

---

### Installing

##### Binary releases

Pre-built binaries are provided for Linux and Windows: https://github.com/Tiiffi/mcrcon/releases/latest

##### Via package manager
See https://pkgs.org/download/mcrcon and https://repology.org/project/mcrcon/packages for available packages in various Linux distros (note that some packages might be outdated).

- Fedora: https://packages.fedoraproject.org/pkgs/mcrcon/mcrcon/
- Gentoo: https://packages.gentoo.org/packages/games-util/mcrcon
- Arch: https://aur.archlinux.org/packages/mcrcon/
- NixOS: https://search.nixos.org/packages?show=mcrcon
- Snapcraft: https://snapcraft.io/mcrcon-nsg
- Scoop: https://scoop.sh/#/apps?q=mcrcon

##### Building from sources
```sh
git clone https://github.com/Tiiffi/mcrcon.git
cd mcrcon
make

# install is optional
sudo make install
```
_Check [BUILDING.md](BUILDING.md) for more details._

---

### Usage
mcrcon [OPTIONS] [COMMANDS]

Sends rcon commands to Minecraft server.

```
Option:
  -H            Server address (default: localhost)
  -P            Port (default: 25575)
  -p            Rcon password
  -t            Terminal mode
  -s            Silent mode
  -c            Disable colors
  -r            Output raw packets
  -w            Wait for specified duration (seconds) between each command (1 - 600s)
  -h            Print usage
  -v            Version information
```
Server address, port and password can be set using following environment variables:
```
MCRCON_HOST
MCRCON_PORT
MCRCON_PASS
```
###### Notes:
- mcrcon will start in terminal mode if no commands are given
- Command-line options will override environment variables
- Rcon commands with spaces must be enclosed in quotes

###### Example:
Send three commands ("say", "save-all", "stop") and wait five seconds between the commands:

```sh
mcrcon -H my.minecraft.server -p password -w 5 "say Server is restarting!" save-all stop
```
---

> [!TIP]
>Enable rcon by adding following lines to [```server.properties```](https://minecraft.gamepedia.com/Server.properties) configuration file.
>```
>enable-rcon=true
>rcon.port=25575
>rcon.password=your_rcon_pasword
>```

---

### Contact

* WWW:            https://github.com/Tiiffi/mcrcon/
* MAIL:           tiiffi+mcrcon at gmail
* ISSUES:         https://github.com/Tiiffi/mcrcon/issues/

> [!TIP]
>When reporting issues, please provide the following information:
>
>- Version of mcrcon: Please specify the precise version number
>- Game: Indicate the specific game server you're using (e.g., Minecraft, Valve Source Engine game, ARK, ...)
>- Server version: Provide the exact version of the game server
>- Mods and Extensions: List all mods and extensions used, including their versions
>- Issue Description: Clearly describe the problem you're encountering and the expected behavior.
>- Steps to reproduce
>
>If you're tech-savvy, consider providing a packet capture file (PCAP). Remember to use a fake password.

---

### License

This project is licensed under the zlib License - see the [LICENSE](LICENSE) file for details.
