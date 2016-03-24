                                 _
     ___ _   _ ___ _ __ __ _  __| |
    / __| | | / __| '__/ _` |/ _` |
    \__ \ |_| \__ \ | | (_| | (_| |
    |___/\__, |___/_|  \__, |\__,_|
         |___/            |_|
    
                    « Don't let me down. » -- The Beatles


sysrqd is a small daemon intended to manage
[Linux SysRq](https://en.wikipedia.org/wiki/Magic_SysRq_key) over the network.
Its philosophy is to be very responsive under heavy load and to try to be
somehow reliable. Authentication is made by clear password.

Connection should be made to port 4094.

# Demo

    % telnet localhost 4094
    Trying 127.0.0.1...
    Connected to localhost.localdomain.
    Escape character is '^]'.
    sysrqd password: hello
    sysrq> s
    sysrq> u
    sysrq> q

# Warning

Please, be careful if you use 'e' (tErm) and 'i' (kIll). This will kill all
processes, including sysrqd!
