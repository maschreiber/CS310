package edu.duke.raft;

import java.util.Timer;

/*
 * Leader's tasks:
 * 1) Send periodic heartbeat to prevent timeout
 * 2) command received from client: append entry to local log, respond after entry applied to state machine
 * 3) If last log index > nextIndex for a follower send AppendEntries RPC with log entries
 * 4) If response contains term T > currentTerm, set currentTerm = T, covert to follower
 */

public class LeaderMode extends RaftMode {
	
	private Timer timer;
	private int timeout;
	private int timerID;
	
	
  public void go () {
    synchronized (mLock) {
      int term = mConfig.getCurrentTerm();
      System.out.println ("S" + 
			  mID + 
			  "." + 
			  term + 
			  ": switched to leader mode.");
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
   * switch into follower if timer timeout
   *  @param id of the timer that timed out
   */
  public void handleTimeout (int timerID) {
		synchronized (mLock) {
			FollwerMode follower - new FollowerMode();
			if (timerID == mID) {
				timer.cancel();
				RaftServerImpl.setMode(follower);
			}
		}
  }
}
