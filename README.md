# TCP-IP Stack Implementation
## How to use

Inside each module, there is a `.sh` file to compile that specific client. Each upper layer includes the inferior one. Inside the folder of the each module you can execute:
```
$ chmod +x .name_of_script.sh
$ ./.name_of_script.sh
```
To compile the module client/server. The compile script makes the compiled executable, so you can just execute it. For example:

```
$ cd udp
$ chmod +x compile.sh
$ ./compile.sh
$ ./udp_client
```
At the moment, ip addresses are hardcoded, so they need to be carefully reviewed before using the stack.