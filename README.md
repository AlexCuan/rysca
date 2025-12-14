# Implementacion TCP-IP-Stack
## Como usar

Dentro de cada módulo hay un archivo `.sh` para compilar ese cliente específico. Cada capa superior incluye la inferior. Dentro de la carpeta de cada módulo se puede ejecutar:
```
$ chmod +x .name_of_script.sh
$ ./.name_of_script.sh
```

Para compilar el módulo cliente/servidor. El script de compilación crea el ejecutable compilado, por lo que solo tienes que ejecutarlo. Por ejemplo:
```
$ cd udp
$ chmod +x compile.sh
$ ./compile.sh
$ /tmp/udp_client [arguments]
```

# Particularidades

- Los modulos IPv4 y UDP tienen checksum integrados. En los clientes, si se le agrega al final de la peticion
el argument `-e` se envia el paquete con un error en el checksum. UDP permite que llegue el paquete al destino,
aunque el servidor lo descarta (debido a que no pasa la comprobacion de checksum); sin embargo, si ip tiene
corrupto el checksum, ni siquiera es enrutado una vez llega al primer router.
- Con udp pasa algo en el entorno virtual y es que los paquetes multicast ripv2 de los routers llegan con el checksum calculado parcialmente.
Una vez dentro del stack, el checksum que le corresponderia tener es calculado correctamente (comprobado con wireshark), 
pero al no ser el que traian los paquetes, son descartados. Esto puede ser debido a que el codigo fue desarrollado en un 
entorno virtualizado, y en un entorno real parte del checksum sea calculado en hardware. De todas formas, se incluye un 
argumento en el servidor ripv2 para desactivar la comprobacion de checksum hasta probarlo en hardware real
- Poison reverse y split horizon estan implementados de una manera muy agresiva ya que solamente se maneja una interfaz
y un enlace en el programa
