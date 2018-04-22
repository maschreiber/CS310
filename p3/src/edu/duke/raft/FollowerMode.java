package edu.duke.raft;

import java.util.Timer;
import java.util.concurrent.ThreadLocalRandom;

public class FollowerMode extends RaftMode {

    private Timer heartbeatTimer;
    
    private void startTimer() {
        if (heartbeatTimer != null)
            heartbeatTimer.cancel();
        // Timeouts (one random, one user set)
        int randomTime = ThreadLocalRandom.current().nextInt(ELECTION_TIMEOUT_MIN, ELECTION_TIMEOUT_MAX + 1);
        int overrideTime = mConfig.getTimeoutOverride();

        // Prioritize validly set user timeouts over randomized timeout.
        int timeout = (overrideTime <= 0) ? randomTime : overrideTime;
        heartbeatTimer = super.scheduleTimer(timeout, 0);
    }
    
    public void go () {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            System.out.println ("S" +
                    mID +
                    "." +
                    term +
                    ": switched to follower mode.");
            startTimer();
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
            int lastLogTerm = mLog.getLastTerm();
            int lastLogIndex = mLog.getLastIndex();
            int votedFor = mConfig.getVotedFor();

            if (term > candidateTerm)
                return term;

            boolean logUpdated;
            if (candidateLastLogTerm > lastLogTerm)
                logUpdated = true;
            else if ((candidateLastLogTerm == lastLogTerm) && (candidateLastLogIndex >= lastLogIndex))
                logUpdated = true;
            else
                logUpdated = false;

            boolean voteCasted = (votedFor != 0 && votedFor != candidateID);

            // If the candidate's term is strictly greater, we grant vote as long as log is updated.
            if (logUpdated) {
                if (term < candidateTerm) {
                    mConfig.setCurrentTerm(candidateTerm, candidateID);
                    startTimer();
                    return 0;
                } else if ((term == candidateTerm) && (!voteCasted)) {
                    // We also vote for the candidate, since we have a vote to give.
                    startTimer();
                    return 0;
                } else {
                    // The follower already casted vote to another candidate, so we deny vote.
                    return term;
                }
            } else {
                if (term < candidateTerm) {
                    mConfig.setCurrentTerm(candidateTerm, 0);;
                    startTimer();
                    return 0;
                } else if ((term == candidateTerm) && (!voteCasted)) {
                    return term;
                } else {
                    return term;
                }
            }
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

            if (leaderTerm < term)
                return term;
            
            // We update the current term to the leader's term since it is larger or equal to ours.
            mConfig.setCurrentTerm(leaderTerm, 0);

            // Recognize that the leader is valid, so we hold its heartbeat valid.
            startTimer();
            
            // Error handling.
            Entry entry;
            try {
                entry = mLog.getEntry(leaderPrevLogIndex);
                if (entry == null)
                    return term;
                else if (entry.term != leaderPrevLogTerm)
                    return term;
            } catch (Exception e) {
                System.out.println(e);
            }
            
            int insertCode = mLog.insert(entries, leaderPrevLogIndex, leaderPrevLogTerm);
            if (insertCode == -1) {
                System.out.println("This case should have been handled already. Strange.");
                return term;
            }
             
            // From RAFT paper    
            if (leaderCommit > mCommitIndex) {
                mCommitIndex = Math.min(leaderCommit, mLog.getLastIndex());
            }
                
            return 0;
        }
    }
    
// @param id of the timer that timed out
    public void handleTimeout (int timerID) {
        synchronized (mLock) {
            CandidateMode candidate = new CandidateMode();
            if (timerID == 0) {
                heartbeatTimer.cancel();
                RaftServerImpl.setMode(candidate);
            }
        }
    }
}

