package edu.duke.raft;

public class CandidateMode extends RaftMode {
  public void go () {
    synchronized (mLock) {
      //when a server first becomes a candidate, it increments its term
      int term = mConfig.getCurrentTerm ();
      term += 1;
      mConfig.setCurrentTerm (term,mID);

      //timer that periodically checks if majority has voted for this candidate


      //sends request votes RPC to all other servers in parallel
      for (int id = 0; id < mConfig.getNumServers (); id++){
          remoteRequestVote (id, term, mID, mLog.getLastIndex(),mLog.getLastTerm());
      }


      System.out.println ("S" +
			  mID +
			  "." +
			  term +
			  ": switched to candidate mode.");
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
      int result = term;
      return result;
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
    }
  }
}
