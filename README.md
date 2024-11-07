# mcrcon

mcrcon is console based Minecraft [rcon](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol) client for remote administration and server maintenance scripts.

---

### Installing:

##### via package manager:
See https://pkgs.org/download/mcrcon and https://repology.org/project/mcrcon/packages for available packages in various Linux distros (note that some packages might be outdated).

- Fedora: https://packages.fedoraproject.org/pkgs/mcrcon/mcrcon/
- Gentoo: https://packages.gentoo.org/packages/games-util/mcrcon
- Arch: https://aur.archlinux.org/packages/mcrcon/
- NixOS: https://search.nixos.org/packages?show=mcrcon
- Snapcraft: https://snapcraft.io/mcrcon-nsg
- Scoop: https://scoop.sh/#/apps?q=mcrcon

##### building from sources:
```sh
git clone https://github.com/Tiiffi/mcrcon.git
cd mcrcon
make

# install is optional
sudo make install
```
Check [INSTALL.md](INSTALL.md) for more details.

Precompiled binaries*: https://github.com/Tiiffi/mcrcon/releases/latest

<sub>*Binaries are provided for Linux and Windows.</sub>

---

### Usage:
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

Example:
> Send three commands ("say", "save-all", "stop") and wait five seconds between the commands.

  ```mcrcon -H my.minecraft.server -p password -w 5 "say Server is restarting!" save-all stop```

---

##### Enable rcon on server
Remember to enable rcon by adding following lines to [```server.properties```](https://minecraft.gamepedia.com/Server.properties) file.
```
enable-rcon=true
rcon.port=25575
rcon.password=your_rcon_pasword
```

---

##### Contact:

* WWW:            https://github.com/Tiiffi/mcrcon/
* MAIL:           tiiffi+mcrcon at gmail
* BUG REPORTS:    https://github.com/Tiiffi/mcrcon/issues/

---

### License

This project is licensed under the zlib License - see the [LICENSE](LICENSE) file for details.

---
