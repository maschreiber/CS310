package edu.duke.raft;

import java.util.Timer;

public class LeaderMode extends RaftMode {
    private Timer timer;  //heartbeat timer
    private int tID;
    private int[] nextIndex;
    private int[] matchIndex; //not used

    public void go () {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            int numServers = mConfig.getNumServers();

            RaftResponses.setTerm(term); //double check if candidate did not do this
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
              //did not initiate to lastIndex + 1 because followerMode checks if entry is empty
              //if empty, then return 0 for heartbeat and will not deincrement nextIndex
                nextIndex[serverID + 1] = mLog.getLastIndex();
                matchIndex[serverID + 1] = 0;
            }
            //System.out.println("When leader server " + mID + " is initialized it has last log index of " + mLog.getLastIndex() +
              //  " and the entry there is ");

            //send empty heartbeat msgs
            for (int i = 1; i < mConfig.getNumServers() + 1; i++) {
                remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), -1, new Entry[0], mCommitIndex);
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

            if (!logUpdated) {
                mConfig.setCurrentTerm(candidateTerm, 0);
                timer.cancel();
                RaftServerImpl.setMode(new FollowerMode());
                return candidateTerm;
            }
            mConfig.setCurrentTerm(candidateTerm, candidateID);
            timer.cancel();
            RaftServerImpl.setMode(new FollowerMode());
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
                timer.cancel();
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
                timer.cancel();
                RaftServerImpl.setMode(new FollowerMode());
                return 0;
            } else {
                return term;
            }

        }
    }


    public void handleTimeout (int timerID) {
        synchronized (mLock) {
            /*heartbeat timer timed out:
             * 1) check responses from each timer cycle, update nextIndex, if should revert to follower etc
             * 2) start new heartbeatTimer
             * 3) Send new appendRPC = heartbeat or repair Log
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
                    if (followerResponses[i] > 0) {
                        //System.out.println("OVERLOAD OVERLOAD!!! " + i + " response is " + followerResponses[i] + " and the nextIndex before decrement at 168 is " + nextIndex[i]);
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
                        int prevLogIndex = nextIndex[i]-1;
                        int prevLogTerm = mLog.getEntry(nextIndex[i]-1).term;

                        if ( (nextIndex[i] > -1) && (nextIndex[i] <= mLog.getLastIndex() ) ) {
                            //repair log
                            Entry[] entry = new Entry[mLog.getLastIndex() - nextIndex[i] + 1];
                            for (int index = 0; index < entry.length; index++) {
                                entry[index] = mLog.getEntry(nextIndex[i] + index);
                            }
                            remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, prevLogIndex, prevLogTerm, entry, mCommitIndex);

                            }
                        else { //empty heartbeat
                            remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, prevLogIndex, prevLogTerm, new Entry[0], mCommitIndex);
                        }
                    }

                    return;
                }

                //finds a follower with a higher term, leader revert to follower
                mConfig.setCurrentTerm(followerTerm, 0);
                timer.cancel();
                RaftServerImpl.setMode(new FollowerMode());
                

            }
        }
    }

}
