# Simple-File-Transfer

Use it to transfer your files between two different linux host.

## HOWTOWORK
Run this program in server mode on host A, then run the same program in client mode on host B to interact with server.

For example, I would like to transfer a file called ***winter.mp4*** from host A whose ip is ***192.168.100.5*** to host B. I can run it by typing command `./sft.out` on A then type command `./sft.out -g winter.mp4 192.168.100.5:9007` to download the file into current directory.

If you wish to send a file such as ***winter.mp3*** from B to A, you can do this by typing command `./sft.out -f ./winter.mp3 192.168.100.5:9007`. The program will display detailed information whether its operation is success or not.

You can also send a message from B to A by typing `./sft.out -m hello,winter! 192.168.100.5:9007`.

This program supports json configuration. You can custom the locations to store log files and where stores files can be sent and files received to be stored at. If locations are not specified, the default location is program's working directory.

The supported key fields and examples are presented below:
```json
{
	"FileReceived" : "./FileReceived/" ,
	"FileToSend" : "./FileToSend/" ,
	"LogPath" : "./Logs/"
}
```
or
```json
{
	"FileReceived" : null ,
	"FileToSend" : null ,
	"LogPath" : "./SOMEWHEREELSE/"
}
```
Note that the specified directories will be created if it is not exist.


## HOWTOBUILD

Type the commands below in your terminal:

`git clone https://github.com/greatmfc/simple-file-transfer`

`cd simple-file-transfer/`

 `make`  or  `make testing`  for debug

  The executable file which Makefile generates is named as ***sft.out*** or ***test.out***.

## HOWTOUSE

Type `./sft.out -h` for help.

```c
usage:
	"Server mode:\n"
	"    ./sft.out [option]\n"
	"Client mode:\n"
	"    ./sft.out [option] [argument] ip:port\n"
	"Options: \n"
	"    General:\n"
	"        -h             This information.\n"
	"        -v             Display version.\n"
	"    In server mode:\n"
	"        -n             No log file will be created.\n"
	"    In client mode:\n"
	"        -f             File mode for sending file. Argument is your file's path.\n"
	"        -g             Fetch file from server. Argument is the file name on server.\n"
	"        -m             Message mode for sending messages. Argument is your content.\n"
	"Arguments: \n"
	"        -f             [file_path]\n"
	"        -g             [file_name]\n"
	"        -m             [contents]\n"
	"Examples:\n"
	"    ./sft.out -f ./file 255.255.255.0:8888\n"
	"    ./sft.out -g file_name 255.255.255.0:8888\n"
	"    ./sft.out -m hello,world! 255.255.255.0:8888\n"
```

