package edu.duke.raft;

import java.util.Timer;
import java.util.concurrent.ThreadLocalRandom;

/*
 * Leader's tasks:
 * 1) Send periodic heartbeat to prevent timeout
 * 2) command received from client: append entry to local log, respond after entry applied to state machine
 * 3) If last log index > nextIndex for a follower send AppendEntries RPC with log entries
 * 4) If response contains term T > currentTerm, set currentTerm = T, covert to follower
 */

public class LeaderMode extends RaftMode {
    
    private Timer heartbeatTimer;
    private int heartbeatTimeout;
    private int heartbeatTimerID;
    private int[] nextIndex;
    private int[] matchIndex;
    
    
    public void go () {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            
            System.out.println ("S" +
                                mID +
                                "." +
                                term +
                                ": switched to leader mode.");
            
            //set heartbeat timer
            heartbeatTimer = startHeartBeatTimer();
            
            int numServer = mConfig.getNumServers();
            
            
            
            nextIndex = new int[numServer + 1];
            matchIndex = new int[numServer + 1];
            for (int server_id = 1; server_id < numServer + 1; server_id++){
                //initialize nextIndex to leader last log index + 1
                nextIndex[server_id] = mLog.getLastIndex() + 1;
                //initialize matchIndex to 0
                matchIndex[server_id] = 0;
            }
            //nextindex: for each server, index of the next log entry to send to that server
            //matchindex: for each server index of highest log entry known to be replicated on server
            
            //send initial empty AppendEntries RPCs to each server
            for (int server_id = 0; server_id < mConfig.getNumServers(); server_id++){
                //Don't send it to the leader/self
                if (server_id != mID){
                    remoteAppendEntries(server_id + 1, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm(), new Entry[0], mCommitIndex);
                }
            }
            
            
        }
    }
    
    private Timer startHeartBeatTimer() {
        synchronized (mLock) {
            if (heartbeatTimer != null)
                heartbeatTimer.cancel();
            // Timeouts (one heartbeat, one user set)
            int heartbeatTime = HEARTBEAT_INTERVAL;
            int overrideTime = mConfig.getTimeoutOverride();
            
            // Prioritize validly set user timeouts over randomized timeout.
            heartbeatTimeout = (overrideTime <= 0) ? heartbeatTime : overrideTime;
            heartbeatTimerID = mID * 7;
            
            return super.scheduleTimer(heartbeatTimeout, heartbeatTimerID);
        }
    }
    
    
    
    // @param candidate’s term
    // @param candidate requesting vote
    // @param index of candidate’s last log entry
    // @param term of candidate’s last log entry
    // @return 0, if server votes for candidate; otherwise, server's
    // current term
    public int requestVote (int candidateTerm,
                            int candidateID,
                            int lastLogIndex,
                            int lastLogTerm) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm ();
            int vote = term;
            return vote;
        }
    }
    
    
    // @param leader’s term
    // @param current leader
    // @param index of log entry before entries to append
    // @param term of log entry before entries to append
    // @param entries to append (in order of 0 to append.length-1)
    // @param index of highest committed entry
    // @return 0, if server appended entries; otherwise, server's
    // current term
    public int appendEntries (int leaderTerm,
                              int leaderID,
                              int prevLogIndex,
                              int prevLogTerm,
                              Entry[] entries,
                              int leaderCommit) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm ();
            int result = term;
            return result;
            
        }
    }
    
    /*
     * send heartbeat messages if timeout
     *
     *  @param id of the timer that timed out
     */
    public void handleTimeout (int timerID) {
        synchronized (mLock) {
            /*heartbeat timer timed out:
             * send requestAppendEntries empty to all followers
             * start new heartbeatTimer
             */
            if (timerID == heartbeatTimerID) {
                
                //check if the append entries rpc reponse has a term higher than leader
                int[] appendResponses = RaftResponse.getAppendResponses(mConfig.getCurrentTerm());
                for (int i = 1; i < appendResponses +1; i++){
                    //if response is 0, server appended entries
                    //otherwise, server's curent term, append RPC failed
                    if (appendResponses[i] <= 0){
                        break;
                    }
                    if (appendResponses[i] > 0){
                        int followerTerm = appendResponses[i];
                        //if follwer term is larger than current term/leader term
                        if (followerTerm > mConfig.currentTerm()){
                            
                            //stop this heartbeat timer
                            heartbeatTimer.cancel();
                            
                            //update current term to the higher term
                            mConfig.setCurrentTerm(followerTerm);
                            
                            //leader convert into follower, this term ends, start new election
                            RaftServerImpl.setMode(new FollowerMode());
                            return;
                        }
                    }
                }
                
                
                //send appendRPC periodicaly here
                
                //start new heartbeatTimer
                heartbeatTimer = startHeartBeatTimer();
                
            }
        }
    }
}
