## APD Homework #3 - BitTorrent
##### Implemented by Toaca Alexandra Simona, 332CA

#### Implementation details

#### Types of MPI messages
- Between the **tracker and a peer** -> tracker_msg:
    ```c
    typedef struct {
        char msg[MAX_MESSAGE];
        int segment_index;
        long peers;        
    } tracker_msg_t;
    ```
    The message can either be a filename (for requesting a file's segments and peers, or for the response to that request), or a segment's hash, along with its index within the file and the peers that own it.

    The **peers** are stored as set bits within the number: for example, if peer 1 owns a segment, the bit at index 1 is 1: 00000..0010. This saves space and is fast, enabling storing different peers for each segment. The only drawback is that a **long** can only store 63 peers, but if needed, a bitset or maybe a char array could accomodate this functionality.

- Between **peers**: a char array of HASH_SIZE (The request for a segment is its hash, the response from the uploader is "ACK").

#### Communication in MPI
- To organize message-passing I used **tags** for differentiating between types of messages.
- For example, when receiving a message, the tracker knows via its tag what it signifies. A PEER_REQUEST tag means the peer asked for a file's segments and peers. The rest of the information needed resides in the tracker_msg.

#### Tracker's swarm
- The tracker keeps information about each file in a swarm. The swarm is an unordered map, which stores a vector of ```segment_t``` for each file. The segments are each stored at their proper index.
    ```c
    typedef struct {
        std::string hash;
        long peers;
    } segment_t;
    ```
- When a peer update comes, the segment is either added or updated (via the segment_index in tracker_msg) and the peers have bit ```status.MPI_SOURCE``` set to 1.
- The tracker knows which file is currently transmitted by a peer via the **connections map**. At each update/request, the peer first sends the name of the file it updates on/wants, it being stored in this map.

#### Segments and peer choosing algorithm
- The peer decides if it wants a segment based on its **peers** field. If the bit corresponding to the current peer is set to 1, the peer already has the segment.
- My approach for choosing which segments to request is a **randomized rarest-first**.
- Rarest first makes a segment available in multiple places, so the original seed isn't choked with requests.
- I decided to make it randomized so that the scarce pieces don't become common, while the others remain rare.
- For example, peer 1 has a file with {hash1, hash2, hash3, hash4, hash5, hash6, hash7, hash8}, which both peer 2 and 3 want. If 2 and 3 both ask for hashes 1, 2, 3 in order, then the rarest first strategy is useless. A randomized approach means for example that peer 2 asks for hashes 2, 5, 7, 8 and peer 3 asks for hashes 1, 3, 4, 6. Now peer 2 can ask for the rest of the hashes from both peer 1 and peer 3. The same applies for peer 3 and its desired segments.
- To randomize the rarest-first approach, I took around 20 of the rarest segments and shuffled them using a random number generator. Then the peer asks for the first 10, which are basically taken at random. There might be an overlap between segments, but it's fine overall.
- The peer from which to download is chosen using an **uniform int distribution**, so each peer has equal probability of being chosen.

