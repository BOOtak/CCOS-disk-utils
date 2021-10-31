# CCOS-disk-utils
Set of tools for manipulating GRiD OS (CCOS) disk images

## Usage
```
ccos_disk_tool [ -i image | -h ] OPTIONS [-v]

Examples:
ccos_disk_tool -i image -p [-s]
ccos_disk_tool -i image -d
ccos_disk_tool -i image -y dir_name
ccos_disk_tool -i image -a file -n name [-l]
ccos_disk_tool -i src_image -c name -t dest_image [-l]
ccos_disk_tool -i src_image -e old name -n new name [-l]
ccos_disk_tool -i image -r file -n name [-l]
ccos_disk_tool -i image -z name [-l]
ccos_disk_tool -i image --create-new

-i, --image IMAGE        Path to GRiD OS floppy RAW image
-h, --help               Show this message
-v, --verbose            Verbose output

OPTIONS:
-w, --create-new         Create new blank image
-p, --print-contents     Print image contents
-s, --short-format       Use short format in printing contents
                         (80-column compatible, no dates)
-d, --dump-dir           Dump image contents into the current directory
-a, --add-file FILE      Add file to the image
-y, --create-dir NAME    Create new directory
-r, --replace-file FILE  Replace file in the image with the given
                         file, save changes to IMAGE.out
-c, --copy-file NAME     Copy file from one image to another
-e, --rename-file FILE   Rename file to the name passed with -n option
-t, --target-name FILE   Path to image to copy file to
-z, --delete-file FILE   Delete file from the image
-n, --target-name NAME   Replace / delete / copy or add file with the name NAME
                         in the image
-l, --in-place           Write changes to the original image
```

## Examples

### Create new empty image `test.img`

For now, new image size defaults to 360k

```
$ ./ccos_disk_tool -i test.img --create-new
```

### List files in the image in short format
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

Free space: 4608 bytes.
```

### Remove file `PhoneLink~Device~` in the `CCOS315.IMG`, save file under `CCOS315.IMG.out`

```bash
./ccos_disk_tool -i CCOS315.IMG -z PhoneLink~Device~
```

### Copy file `Executive~Run~` from `CCOS315.IMG` to `GRIDOS.IMG`

```bash
# For now, we don't support replacing files in the image, so, delete file first
# Use -l key to save new image under the same name
./ccos_disk_tool -i GRIDOS.IMG -z Executive~Run~ -l
# Now, copy file over
./ccos_disk_tool -i CCOS315.IMG -c Executive~Run~ -t GRIDOS.IMG -l
```

### Add file `MAIN.RUN` as `Main~Run~` to `GRIDOS.IMG`

```bash
./ccos_disk_tool -i GRIDOS.IMG -a MAIN.RUN -n Main~Run~ -l
```
