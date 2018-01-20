The source code of client is in Client1/Client1/main.c
The source code of server is in Server1/Server1/main.c
The snapshots of client and server running output is Client_Snapshot.jpeg, Server_Snapshot1.jpeg and Server_Snapshot2.jpeg. (Server_Snapshot2.jpeg is subsequent part of Server_Snapshot1.jpeg).

The client sends 5 packets, representing:
0-correct packet
1-duplicate packet (duplicate with the first packet)
2-out of sequence packet (segment number is incorrect)
3-packet without end (missing end of packet id)
4-length mismatch packet (length of payload is incorrect)
respectively.

The server receives these packets and check the content of the packets, send corresponding ack packet or error packet to the client. The client receive the packet from server and display that if the packet is rejected or not. If the server does not send back packet in 3sec, the client resend the packet. After 3 times resend, client stops sending and confirm that server does not respond.