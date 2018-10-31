# Design

## Overall data structures 

For each process, we keep a list of 

MemberStat {
    int id;
    boolean alive;
    string address;
}

MyState {
    leader,
    member,
    non_member
}

## initiation 

By default, leader is the process with lowest id number,
which starts with 1. 

Leader has to be initiated first, the order of each member's
initiation does not matter. 

When a node initiates, it first identifies whether its 
leader, if so, it opens a TCP channel and starts to listen

If its not a leader, it identifies the first process as 
leader and sends a request message to ask for joining the
group.

## heartbeat mechanism

Every 2 seconds, each process will send an `I am alive` 
message to each processes in the `alive processes` list. 

For each process i, if, in the past 4 seconds, there is an
`I am alive` message received from process k, then we mark 
process k to be alive. otherwise we treat it as offline. 

## algorithm

    1) when a new process join the group, it first establish 
        a connection with leader. 
    2) when a connection is established, it sends a join message
       to the leader.
    3) the leader then broadcast message to all  

## fail detector

Every 5 secs, the process will send a heartbeat message to all 
processes in the address list. Message will only be one int long,
which will be the id of the sender. 

Each Member struct will have a field 'has_received_heartbeat'; which
is initiated as true; 

Every 5 secs, the process will check status. if the `has_received_hearbeat`
is true, set it to false. If it is false, then report the process as offline.
