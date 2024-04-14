
# Simple File Transfer

**Easily transfer files between two Linux hosts.**

****

## How It Works
- **Server Mode**: Run the program on host by simply typing `./sft.out` or `./sft.out -n` without logs. It can also act like a normal http server.
- **Client Mode**: Execute the program with option and argument to interact with the server.

**Example**:

- **Download any file from other host**:
 To get `winter.mp4` from Host A (`192.168.100.5`) to Host B, initiate the server on A with `./sft.out`. Then, on B, use `./sft.out -g winter.mp4 192.168.100.5:9007` to download the file.

- **Send any file to other host**:
  To send `winter.mp3` from B to A, ensure A is in server mode, then on B, execute `./sft.out -f ./winter.mp3 192.168.100.5:9007`.

- **Send any messages to other host:**
  Send messages with `./sft.out -m "hello,winter!" 192.168.100.5:9007`.

- **Http server:**
  Place the http page files under http directory which is specified in `sft.json`. Then access `0.0.0.0:9007` or other ip like a normal http server.



**Configuration**: Utilizes JSON for custom settings including log, send, and receive file directories. Defaults to the working directory if unspecified.

**Example Configuration**:
```json
{
    "FileReceived": "./FileReceived",
    "FileToSend": "./FileToSend",
    "LogPath": "./Logs",
    "HttpPath":"./http",
    "DefaultPage": "index.html",
    "ListenPort": 9007
}
```
Directories are created if they do not exist.

****

## Building the Program
Clone and build with the following commands:
```bash
git clone https://github.com/greatmfc/simple-file-transfer
cd simple-file-transfer/
make # or 'make testing' for debug mode
```
Generates `sft.out` or `test.out` executable.

****

## Usage
Run `./sft.out -h` for help. Supports server and client modes with options for file transfer, fetching, and messaging.

**Examples**:
```bash
./sft.out -f ./file 255.255.255.0:8888
./sft.out -g file_name 255.255.255.0:8888
./sft.out -m "hello,world!" 255.255.255.0:8888
```
