package edu.duke.raft;
import java.util.Timer;

public class CandidateMode extends RaftMode {
    private Timer checkVoteTimer;
    final private int CHECK_VOTE_TIME_INTERVAL = 30;
    private Timer callAnotherElectionTimer;
    private boolean callingAnotherElection = false;
    private boolean candidateAlreadySwitchedMode = true;
    
    private void cancelBothTimer () {
        checkVoteTimer.cancel();
        callAnotherElectionTimer.cancel();
    }
    
    private void initiateCheckVoteTimer () {
        checkVoteTimer = scheduleTimer(CHECK_VOTE_TIME_INTERVAL, 0);
    }
    
    private void initiateCallAnotherElectionTimer () {
        if (mConfig.getTimeoutOverride() <= 0) {callAnotherElectionTimer = scheduleTimer((long) Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN, 1);}
        else {callAnotherElectionTimer = scheduleTimer(mConfig.getTimeoutOverride(), 1);}
    }
    
    public void go () {
        synchronized (mLock) {
            // When a server first becomes a candidate, it increments its term.
            int term = mConfig.getCurrentTerm();
            term++;

            // We set that incremented term, and also "vote" for ourselves in the process.
            mConfig.setCurrentTerm(term, mID);
            initiateCheckVoteTimer();
            initiateCallAnotherElectionTimer();

            createElection();
        }
    }
    
    private void createElection() {
        int term = mConfig.getCurrentTerm();

        RaftResponses.setTerm(term);
        RaftResponses.clearVotes(term);
            
        int serverID = 0;
        while (serverID != mConfig.getNumServers()) {                
            remoteRequestVote(serverID, term, mID, mLog.getLastIndex(), mLog.getLastTerm());
            serverID++;
        }
            
        if (callingAnotherElection) {callingAnotherElection = false;}
        else {System.out.println("S" + mID + "." + term + ": switched to candidate mode.");}
            
        candidateAlreadySwitchedMode = false;
    }

    // @param candidate’s term
    // @param candidate requesting vote
    // @param index of candidate’s last log entry
    // @param term of candidate’s last log entry
    // @return 0, if server votes for candidate; otherwise, server's current term
    public int requestVote (int candidateTerm, int candidateID, int candidateLastLogIndex, int candidateLastLogTerm) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();

            // Vote for itself
            if (candidateID == mID)
                return 0;

            // According to Piazza @854, you only vote for the other candidate if their term is strictly greater.
            // If our term is at least as large as the candidate's, do not cast your vote for them.
            if (term >= candidateTerm)
                return term;

            // We can cancel timer permanently, because we will end up transitioning this server to follower mode.
            cancelBothTimer();

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
    
    
    // @param leader’s term
    // @param current leader
    // @param index of log entry before entries to append
    // @param term of log entry before entries to append
    // @param entries to append (in order of 0 to append.length-1)
    // @param index of highest committed entry
    // @return 0, if server appended entries; otherwise, server's current term
    public int appendEntries (int leaderTerm, int leaderID, int prevLogIndex, int prevLogTerm, Entry[] entries, int leaderCommit) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();

            // Will not accept entries to append from a leader with a lower term.
            if (leaderTerm < term)
                return term;
            
    
            mConfig.setCurrentTerm(leaderTerm, leaderID);
            cancelBothTimer();
                
            FollowerMode follower = new FollowerMode();
            RaftServerImpl.setMode(follower);
            return -1;
        }
    }
    
    // @param id of the timer that timed out
    public void handleTimeout (int timerID) {
        synchronized (mLock) {            
            if (timerID == 0) {
            
                int[] votes = RaftResponses.getVotes(mConfig.getCurrentTerm());
                int count = 0;
                int vote;
                
                for (int i = 1; i < votes.length; i++) {
                    vote = votes[i];
                    
                    if (vote > mConfig.getCurrentTerm()) {
                        // System.out.println("point 3");
                        cancelBothTimer();
                        mConfig.setCurrentTerm(vote, i);
                        
                        if (candidateAlreadySwitchedMode == false) {
                            candidateAlreadySwitchedMode = true;
                            RaftServerImpl.setMode(new FollowerMode());
                        }
                        
                        return;
                    }
                    
                    if (vote == 0) {count++;}  /*System.out.print(vote + " ");*/
                }
                
                // System.out.println();
                
                if (count *2 > mConfig.getNumServers()) {
                    cancelBothTimer();
                    
                    if (candidateAlreadySwitchedMode == false) {
                        candidateAlreadySwitchedMode = true;
                        RaftServerImpl.setMode(new LeaderMode());
                    }
                    
                    return;
                }
                else {
                    RaftResponses.clearVotes(mConfig.getCurrentTerm());
                    
                    for (int i = 1; i < mConfig.getNumServers() + 1; i++) {
                        remoteRequestVote(i, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm());
                    }
                    
                    checkVoteTimer.cancel();
                    initiateCheckVoteTimer();
                }
            }
            else if (timerID == 1) {
                checkVoteTimer.cancel();
                callAnotherElectionTimer.cancel();
                callingAnotherElection = true;
                
                if (candidateAlreadySwitchedMode == false) {
                    candidateAlreadySwitchedMode = true;
                    go();
                }
            }
            
            // else {System.out.println(mID + ": wrong timer");}
        }
    }
}
