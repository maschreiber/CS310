package edu.duke.raft;

import java.util.Timer;
import java.util.concurrent.ThreadLocalRandom;

public class CandidateMode extends RaftMode {

    private Timer voteTimer;
    private Timer electionTimer;
    private boolean firstTimeCandidate = true;
    
    private void startTimer () {
        if (voteTimer != null)
            voteTimer.cancel();
        if (electionTimer != null)
            electionTimer.cancel();

        voteTimer = super.scheduleTimer(25, 0);

        // Timeouts (one random, one user set)
        int randomTime = ThreadLocalRandom.current().nextInt(ELECTION_TIMEOUT_MIN, ELECTION_TIMEOUT_MAX + 1);
        int overrideTime = mConfig.getTimeoutOverride();

        // Prioritize validly set user timeouts over randomized timeout.
        int timeout = (overrideTime <= 0) ? randomTime : overrideTime;
        electionTimer = super.scheduleTimer(timeout, 1);
    }
    
    public void go () {
        synchronized (mLock) {
            // When a server first becomes a candidate, it increments its term.
            int term = mConfig.getCurrentTerm();
            term++;

            if (firstTimeCandidate) {
                System.out.println ("S" +
                    mID +
                    "." +
                    term +
                    ": switched to candidate mode.");
            }
             /*System.out.println("This is our candidate's log " + "S" + 
            mID + 
            "." + 
            term + 
            ": Log " + 
            mLog);*/

            // We set that incremented term, and also "vote" for ourselves in the process.
            mConfig.setCurrentTerm(term, mID);
            startTimer();
            startElection();
        }
    }
    
    private void startElection() {
        int term = mConfig.getCurrentTerm();

        RaftResponses.setTerm(term);
        RaftResponses.clearVotes(term);
            
        int serverID = 0;
        while (serverID != mConfig.getNumServers()) {                
            remoteRequestVote(serverID+1, term, mID, mLog.getLastIndex(), mLog.getLastTerm());
            serverID++;
        }
    }

    // @param candidate’s term
    // @param candidate requesting vote
    // @param index of candidate’s last log entry
    // @param term of candidate’s last log entry
    // @return 0, if server votes for candidate; otherwise, server's current term
    public int requestVote (int candidateTerm, 
                            int candidateID, 
                            int candidateLastLogIndex, 
                            int candidateLastLogTerm) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();

            // Vote for itself
            if (mID == candidateID)
                return 0;

            // According to Piazza @854, you only vote for the other candidate if their term is strictly greater.
            // If our term is at least as large as the candidate's, do not cast your vote for them.
            if (term >= candidateTerm)
                return term;

            // We can cancel timer permanently, because we will end up transitioning this server to follower mode.
            voteTimer.cancel();
            electionTimer.cancel();

            int lastLogIndex = mLog.getLastIndex();
            int lastLogTerm = mLog.getLastTerm();

            //System.out.println("C-RV server " + mID + " Candidate term is " + candidateTerm + " and candidateID is " + candidateID + "with candidateLastLogIndex" + candidateLastLogIndex + " and candidateLastLogTerm " + candidateLastLogTerm);

            boolean logUpdated;
            if (candidateLastLogTerm > lastLogTerm)
                logUpdated = true;
            else if ((candidateLastLogTerm == lastLogTerm) && (candidateLastLogIndex >= lastLogIndex))
                logUpdated = true;
            else
                logUpdated = false;

            if (!logUpdated) {
                mConfig.setCurrentTerm(candidateTerm, 0);
                voteTimer.cancel();
                electionTimer.cancel();
                RaftServerImpl.setMode(new FollowerMode());
                return candidateTerm;
            }

            mConfig.setCurrentTerm(candidateTerm, candidateID);
            voteTimer.cancel();
            electionTimer.cancel();
            RaftServerImpl.setMode(new FollowerMode());
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
    public int appendEntries (int leaderTerm, 
                              int leaderID, 
                              int leaderPrevLogIndex, 
                              int leaderPrevLogTerm, 
                              Entry[] entries, 
                              int leaderCommit) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();

            // Will not accept entries to append from a leader with a lower term.
            if (leaderTerm < term)
                return term;
            
    
            mConfig.setCurrentTerm(leaderTerm, leaderID);
            voteTimer.cancel();
            electionTimer.cancel();
            RaftServerImpl.setMode(new FollowerMode());
            return leaderTerm;
        }
    }
    
    // @param id of the timer that timed out
    public void handleTimeout (int timerID) {
        synchronized (mLock) {            
            if (timerID == 0) {
                handleVotingTimeout();
            }
            else if (timerID == 1) {
                handleElectionTimeout();
            }
        }
    }

    private void handleVotingTimeout() {
        int term = mConfig.getCurrentTerm();
        int[] votes = RaftResponses.getVotes(term);
        int votesForMe = 0;
        int max_term = Integer.MIN_VALUE;

        for (int vote : votes) {
            if (vote > term) {
                max_term = Math.max(max_term, vote);
            } else if (vote == 0) {
                votesForMe++;
            }
        }
        int index = 0;
        if (max_term > term) {
            for (int i = 1; i < votes.length; i++) {
                if (votes[i] == max_term) {
                    index = i;
                    break;
                }
            }
            mConfig.setCurrentTerm(max_term, index);
            voteTimer.cancel();
            electionTimer.cancel();
            RaftServerImpl.setMode(new FollowerMode());
            return;
        }
        if (mConfig.getNumServers() >= votesForMe*2) {
            voteTimer.cancel();
            voteTimer = super.scheduleTimer(25, 0);
        }
        else {
            voteTimer.cancel();
            electionTimer.cancel();
            RaftServerImpl.setMode(new LeaderMode());
            return;
        }
    }

    private void handleElectionTimeout() {
        voteTimer.cancel();
        electionTimer.cancel();
        firstTimeCandidate = false;
        this.go();
    }
}
