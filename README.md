# Simple-File-Transfer

Use it to transfer your files between two different linux host.

## HOWTOBUILD

Type the codes below in your terminal:

`git clone https://github.com/greatmfc/simple-file-transfer`

`cd simple-file-transfer/`

 `make`  or  `make testing`  for debug

  The executable file Makefile generates is named as ***sft.out*** or ***test.out***.

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

