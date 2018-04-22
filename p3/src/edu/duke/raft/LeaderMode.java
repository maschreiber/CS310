package edu.duke.raft;

import java.util.Timer;

public class LeaderMode extends RaftMode {
    private Timer timer;
    private int tID;
    private int[] nextIndex;
    private int[] matchIndex;

    public void go () {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            int numServers = mConfig.getNumServers();

            RaftResponses.setTerm(term);
            RaftResponses.clearAppendResponses(term);

            tID = 11;

            startTimer();

            System.out.println ("S" +
                    mID +
                    "." +
                    term +
                    ": switched to leader mode.");

            nextIndex = new int[numServers + 1];
            matchIndex = new int[numServers + 1];
            for(int serverID = 0; serverID < numServers; serverID++){
                nextIndex[serverID + 1] = mLog.getLastIndex() + 1;
                matchIndex[serverID + 1] = 0;
            }
            
            //send empty heartbeat msgs
            for (int i = 1; i < mConfig.getNumServers() + 1; i++) {
                remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, nextIndex[i]-1, -1, new Entry[0], mCommitIndex);
            }

        }
    }

    private void startTimer() {
        if (timer != null)
            timer.cancel();
        timer = scheduleTimer(HEARTBEAT_INTERVAL, tID);
    }

   
    public int requestVote (int candidateTerm,
                            int candidateID,
                            int candidateLastLogIndex,
                            int candidateLastLogTerm) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            
            // The leader is dominant over the candidate. The candidate must have a strictly higher
            // term to get the leader to step down.
            if (term >= candidateTerm) {
                return term;
            }

            timer.cancel();

            int lastLogIndex = mLog.getLastIndex();
            int lastLogTerm = mLog.getLastTerm();

            boolean logUpdated;
            if (candidateLastLogTerm > lastLogTerm)
                logUpdated = true;
            else if ((candidateLastLogTerm == lastLogTerm) && (candidateLastLogIndex >= lastLogIndex))
                logUpdated = true;
            else
                logUpdated = false;

            FollowerMode follower = new FollowerMode();

            if (!logUpdated) {
                mConfig.setCurrentTerm(candidateTerm, 0);
                RaftServerImpl.setMode(follower);
                return candidateTerm;
            }
            mConfig.setCurrentTerm(candidateTerm, candidateID);
            RaftServerImpl.setMode(follower);
            return 0;
        }
    }

    public int appendEntries (int leaderTerm, int leaderID, int leaderPrevLogIndex,
                              int leaderPrevLogTerm,
                              Entry[] entries,
                              int leaderCommit) {
        synchronized (mLock) {
            if (leaderID == mID)
                return 0; // or return -1(UP IN THE AIR)

            int term = mConfig.getCurrentTerm();

            if (term > leaderTerm)
                return term;

            timer.cancel();
            
            // In the case where there is another leader with a great term, yield way.
            if (term < leaderTerm) {
                // update term, step down to be follower
                mConfig.setCurrentTerm(leaderTerm, 0);
                RaftServerImpl.setMode(new FollowerMode());
                return 0; // or return -1 (UP IN THE AIR)
            }
            
            int prevLogIndex = mLog.getLastIndex();
            int prevLogTerm = mLog.getLastTerm();

            // This means that this server and the other leader have equivalent terms.
            // Which one steps down?
            boolean logUpdated;
            if (leaderPrevLogTerm > prevLogTerm)
                logUpdated = true;
            else if ((leaderPrevLogTerm == prevLogTerm) && (leaderPrevLogIndex >= prevLogIndex))
                logUpdated = true;
            else
                logUpdated = false;


            if (logUpdated) {
                RaftServerImpl.setMode(new FollowerMode());
                return 0;
            } else {
                return term;
            }

        }
    }

    /**
     * @param timerID id of the timer that timed out
     */
    public void handleTimeout (int timerID) {
        synchronized (mLock) {
            /*heartbeat timer timed out:
             * 1) check responses from each timer cycle, update nextIndex, if should revert to follower etc
             * 2) start new heartbeatTimer
             * 3) Send new appendRPC = heartbeat + append Log
             */
            if (timerID == tID) {
                //check this cycle's AppendEntries RPC responses

                //array of AppendEntries RPC responses  
                int[] followerResponses = RaftResponses.getAppendResponses(mConfig.getCurrentTerm());
                
                //if maxFollowerTerm is larger than current in the end, revert to follower
                int term = mConfig.getCurrentTerm();
                int followerTerm = term;
                
                //for each follower response
                for (int i = 1; i < followerResponses.length; i++) {
                    //if follower responses term is larger than the max term
                    if (followerResponses[i] > followerTerm) {
                        followerTerm = followerResponses[i];
                    }
                    //if fails decrement nextIndex and retry
                    if (followerResponses[i] != 0) {
                        nextIndex[i] -= 1; //this will be checked/retried in method sendAppendEntriesRPC()
                    }
                }
                
                //if there exists a follower with higher term than current term, leader revert to follower
                if (term >= followerTerm) {
                    //if leader stays as leader, initiate new cycle
                    RaftResponses.clearAppendResponses(mConfig.getCurrentTerm()); 
                    startTimer();
                    
                    //send new AppendEntries RPC
                    
                    for (int i = 1; i < mConfig.getNumServers() + 1; i++) {
                        if ( (nextIndex[i] > -1) && (nextIndex[i] <= mLog.getLastIndex()) ) {
                            //repair log
                            Entry[] entry = new Entry[mLog.getLastIndex() - nextIndex[i] + 1];
                            for (int index = 0; index <= mLog.getLastIndex() - nextIndex[i]; index++) {
                                entry[0] = mLog.getEntry(index);
                            }
                            remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, nextIndex[i]-1, mLog.getEntry(nextIndex[i]).term, entry, mCommitIndex);
         
                            }else { //empty heartbeat
                            remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, nextIndex[i]-1, -1, new Entry[0], mCommitIndex);
                           } 
                    }

                }else {
                    timer.cancel();
                    mConfig.setCurrentTerm(followerTerm, 0);
                    RaftServerImpl.setMode(new FollowerMode());
                }
                
            }
        }
    }
    
}
