package edu.duke.raft;

import java.util.concurrent.ThreadLocalRandom;

public class FollowerMode extends RaftMode {

  private Timer timer;
  private int timeout;
  private int timerID;

  public void go () {
    synchronized (mLock) {
      int term = 0;
      System.out.println ("S" +
			  mID +
			  "." +
			  term +
			  ": switched to follower mode.");
      timer = startTimer();
    }
  }

  private Timer startTimer() {
    synchronized (mLock) {
      // Timeouts (one random, one user set)
      int randomTime = ThreadLocalRandom.current().nextInt(ELECTION_TIMEOUT_MIN, ELECTION_TIMEOUT_MAX);
      int overrideTime = mConfig.getTimeoutOverride();

      // Prioritize validly set user timeouts over randomized timeout.
      timeout = (overrideTime <= 0) ? randomTime : overrideTime;
      timerID = mID;

      return super.scheduleTimer(timeout, timerID);
    }
  }

  // @param candidate’s term
  // @param candidate requesting vote
  // @param index of candidate’s last log entry
  // @param term of candidate’s last log entry
  // @return 0, if server votes for candidate; otherwise, server's
  // current term
  public int requestVote (int candidateTerm,
  //refined criterion in class
			  int candidateID,
			  int lastLogIndex,
			  int lastLogTerm) {
    synchronized (mLock) {
      int mCurrentTerm = mConfig.getCurrentTerm ();
      int mLogTerm = mLog.getLastTerm();

      /*
       * Conditions that must be met so this server gives its vote to candidate.
       * 1) This server has a lower or equal term than the candidate.
       * 2) This server has a lower or equal log index than the candidate.
       * 3) This server has not voted or already voted.
       */

      // Make sure the server hasn't already voted.
      int votedFor = mConfig.getVotedFor();
      if (votedFor != 0 && votedFor != candidateID)
        return mCurrentTerm;

      // This server shouldn't have a higher term than the candidate.
      if (mCurrentTerm > candidateTerm)
        return mCurrentTerm;

      if (mLogTerm > lastLogTerm) {
        mConfig.setCurrentTerm(candidateTerm, mID);
        return mCurrentTerm;
      }



      if (myServerTerm <= candidateTerm) {
        // If the term of the last entry in this server is higher, deny vote.
        if (myServerLLT > lastLogTerm) {
          return myServerTerm;
        }
      }
      if (lastLogTerm > term){
        return 0;
      }
      else if (lastLogTerm < term){
        return term;
      }
      //otherwise same term, the longer log is latest (5.4.1) index larger?
      else if (lastLogTerm == term){
        if (lastLogIndex > mCommitIndex){
          return 0;
        }
      }
      return term;
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

  // @param id of the timer that timed out
  public void handleTimeout (int timerID) {
    synchronized (mLock) {
      // Switch to candidate mode if timer has timed out.
      CandidateMode candidate = new CandidateMode();
      if (timerID == mID) {
        timer.cancel();
        RaftServerImpl.setMode(candidate);
      }
    }
  }
}
