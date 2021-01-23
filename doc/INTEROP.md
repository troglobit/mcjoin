### Testing with 3rd party software ###

To test `mcjoin` with a 3rd party software, you can use `logger`:

**On Reciver:**
````bash
mcjoin
````

**On Semder:**
````bash
while true; do logger -n 225.1.2.3 -P 1234 "Test message"; sleep 0.5; done
````
#### Result
Since `logger` does not send the expected packets the reciver, it will indicate error

````python
Server (192.168.2.217@wlp2s0)Help | Toggle | Quit]          Sat Jan 23 22:30:14 2021
Source,Group   Plotter                                                          Packets
*,225.1.2.3  \ [         I    I    I    I    I    I    I    I     I    I    I]       11

Time                      Log                                                          
Sat Jan 23 22:29:54 2021  Joining (*,225.1.2.3) on wlp2s0, ifindex: 3, sd: 5
Sat Jan 23 22:30:14 2021  *,225.1.2.3: invalid 11    delay 0     gaps 0     reorder 0
Sat Jan 23 22:30:14 2021  Total: 11 packets
Sat Jan 23 22:30:14 2021  Uptime: 20s
````
