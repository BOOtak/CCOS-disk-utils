# CCOS-disk-utils
Set of tools for manipulating GRiD OS (CCOS) disk images

## Usage
```
ccos_disk_tool { -i <image> | -h } [OPTIONS] [-v]

-i, --image <path>              Path to GRiD OS floppy RAW image
-h, --help                      Show this message
-v, --verbose                   Verbose output

Options are:
-p [-s] | -d | -r <file> [-n <name>] [-l] | -c <file> OPTIONS | -z <file> [-l]

-p, --print-contents            Print image contents
-s, --short-format              Use short format in printing contents
                                (80-column compatible, no dates)
-d, --dump-dir                  Dump image contents into the current directory
-r, --replace-file <filename>   Replace file in the image with the given
                                file, save changes to <path>.new
-n, --target-name <name>        Optionally, replace file <name> in the image
                                instead of basename of file passed with
                                --replace-file
-c, --copy-file <filename>      Copy file between images
-z, --delete-file <filename>    Delete file from the image
-l, --in-place                  Write changes in the original image

Copying options are:
-t <path> -n <name> [-l]

-t, --target-image <filename>   Path to the image to copy file to
-n, --target-name <name>        Name of file to copy
-l, --in-place                  Write changes in the original image
```

## Example

```
$ ./ccos_disk_tool.exe -i CCOS315.IMG -p -s
-------------
|CCOS315.IMG| -  GRiD-OS/Windows 113x, 114x v3.1.5D
-------------

File name                       File type               File size     Version
--------------------------------------------------------------------------------
Programs                        subject                 736           0.0.0
  @SystemErrors                 Text                    3657          3.1.8
  CCOS                          System                  64506         34.1.5
  Common                        Shared                  70192         3.1.0
  Diablo630GPIB                 Printer                 4120          3.1.5
  Diablo630SerialETX/ACK        Printer                 3885          3.1.5
  Do                            Run Com                 2807          3.0.0
  Duplicate Media               Run                     6104          3.1.0
  EpsonFX100GPIB                Printer                 3116          3.1.5
  EpsonFX80GPIB                 Printer                 3116          3.1.5
  EpsonMX100GPIB                Printer                 4056          3.1.5
  Executive                     Run                     39796         3.2.0
  GenericGPIB                   Printer                 1411          3.1.5
  GenericSerialETX/ACK          Printer                 1760          3.1.5
  GenericSerialXON/XOFF         Printer                 1716          3.1.5
  GRiDManager                   Run Sign-on             32039         3.1.0
  HP2225GPIB                    Printer                 4625          3.1.5
  HPGPIB                        Plotter                 3697          3.1.5
  HPSerial                      Plotter                 1477          3.1.5
  Initialize Media              Run                     16465         3.1.0
  MediaRepair                   Run                     18925         3.1.0
  Modem                         Device                  5049          3.1.0
  PhoneLink                     Device                  16442         3.1.6
  Screen.init                   ScreenImage             3320          3.0.0
  ScreenWatch                   Run                     3937          3.1.6
  Serial                        Device                  4678          3.1.5
  Sound                         Device                  1785          3.1.6
  TypeGRiD4x8                   Font                    1378          3.1.5
  TypeGRiD5x8                   Font                    1378          3.1.5
  TypeGRiD6x7                   Font                    1673          3.1.5
  TypeGRiD6x8                   Font                    1905          3.1.5
  User                          Profile                 30            3.1.0
```
