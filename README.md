# Simple-File-Transfer

Use it to transfer your files between two different linux host.

## HOWTOBUILD

Type the codes below in your terminal:

`git clone https://github.com/greatmfc/simple-file-transfer`

`cd simple-file-transfer/`

 `make` (if gcc12 is not installed, then it is `make sft`)

`make install`(optional)

## HOWTOUSE

Type `./sft -h`for help.

```c
usage:
	"server mode:"
	"sft [option]"
	"client mode:"
	"sft [option] [argument] ip:port\n"
	"options: \n"
	"-c				Check mode for identification before receiving.\n"
	"-f				File mode for sending file.Argument is your file's path.\n"
	"-m				Message mode for sending message.Argument is your content.\n"
	"-h				This information.\n"
	"-v				Display version.\n"
	"-n				No log file left behind after finishing program.\n"
	"-g				Fetch file from server.\n"
	"Examples:			./sft -f ./file 255.255.255.0:8888\n"
	"				./sft -m hello,world! 255.255.255.0:8888\n"
	"				./sft -g file_name 255.255.255.0:8888\n"
```

