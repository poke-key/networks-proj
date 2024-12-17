# UDP Go-Back-N Protocol Implementation

A UDP-based file transfer application implementing TCP-like reliability features including three-way handshake, sequence numbering, and Go-Back-N ARQ protocol.

## Compilation

```bash
# Compile server
gcc server.c -o server

# Compile client (provided by TA)
gcc reference_client.c -o client
```

## Usage

1. Start the server:
```bash
./server 8000 input.txt
```
- Port number (e.g. 8000) is required
- input.txt is the file to be transferred

2. Run the client:
```bash 
./client 8000 test_11.txt output.txt
```
- test_11.txt contains test parameters including window size and packet loss simulation
- output.txt is where received file content will be saved

## Core Implementation Highlights

### Three-Way Handshake
```c
// Server waits for SYN, sends SYN-ACK, waits for ACK
if (recv_packet.flag == SYN && recv_packet.ack == 0) {
    // Send SYN-ACK
    send_packet.seq = cur_seq;
    send_packet.ack = client_seq;
    send_packet.flag = SYN_ACK;
}
```

### Go-Back-N Window Management
```c
// Sliding window implementation
while (next_seq_num < base + current_window_size && next_seq_num <= total_bytes) {
    // Send packets within window
    send_packet.seq = next_seq_num;
    sendto(sockfd, &send_packet, sizeof(Packet), 0, ...);
    next_seq_num++;
}
```

### Packet Loss Handling
```c
// Window size reduction on packet loss
if (current_window_size == window_size) {
    current_window_size = window_size / 2;
    if (current_window_size < 1) current_window_size = 1;
}
```

### File Transfer
```c
// Read and send file content in packets
size_t bytes_read = fread(send_packet.payload, 1, MAX_PAYLOAD, file);
send_packet.payload_size = bytes_read;
```

The implementation follows standard networking protocols while adding reliability mechanisms on top of UDP. Test files (test_1.txt through test_11.txt) verify different scenarios including packet loss and window size adjustments.
## Program Overview

The server implements:
- Three-way handshake for connection establishment
- Go-Back-N sliding window protocol
- Dynamic window size adjustment based on packet loss
- File transfer functionality with progress tracking
- Automatic retransmission on packet loss or timeout
- Window size reduction (halving) on packet loss
- Window size restoration after successful transmission windows

The implementation follows standard networking protocols while adding reliability mechanisms on top of UDP. Test files (test_1.txt through test_11.txt) can be used to verify different scenarios including packet loss and window size adjustments.