package edu.duke.raft;

public class CandidateMode extends RaftMode {
	public void go () {
		synchronized (mLock) {
			// When a server first becomes a candidate, it increments its term.
			int term = mConfig.getCurrentTerm();
			term++;
			mConfig.setCurrentTerm(term, mID);

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


	/**
	 * Append entries to local log. Step down to follower if higher term leader presents itself.
	 * 
	 * @param leaaderTerm the current term of the leader
	 * @param leaedrID server ID of the leader
	 * @param leaderPrevLogIndex index of the log entry before entries to append
	 * @param leaderPrevLogTerm term of log entry before entries to append
	 * @param entries to append
	 * 
	 * @return 0 if successful append or step down, server current term if append denied, -1 if log corrupted
	 */
	public int appendEntries (int leaderTerm, int leaderID, int leaderPrevLogIndex,
			int leaderPrevLogTerm, Entry[] entries, int leaderCommit) {
		synchronized (mLock) {
			int term = mConfig.getCurrentTerm ();

			// Will not accept entries to append from a leader with a lower term.
			if (leaderTerm < term)
				return term;
			
			// Step down if leader's term exceeds or matches.
			mConfig.setCurrentTerm(leaderTerm, leaderID);
			electionTimer.cancel();
			RaftServerImpl.setMode(new FollowerMode());
			return 0;
		}
	}

	// @param id of the timer that timed out
	public void handleTimeout (int timerID) {
		synchronized (mLock) {
		}
	}
}
