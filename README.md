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

When a node is first started, it goes into `init` mode, in 
which it establish connections with all its neighbor nodes.
Identify who's alive, and 

    1) if its the leader, it announce itself to all members
       in the member list, which is empty in the beginning.
    2) if its a member, it sends a message to the leader 
        requesting for membership.   