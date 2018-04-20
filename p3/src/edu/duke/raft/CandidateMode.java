package edu.duke.raft;

import java.util.Timer;
import java.util.concurrent.ThreadLocalRandom;

public class CandidateMode extends RaftMode {
	
	private Timer timer;
	private int timeout;
	private int timerID;
	
	public void go() {
		synchronized (mLock) {
			// When a server first becomes a candidate, it increments its term.
			int term = mConfig.getCurrentTerm();
			term++;
			mConfig.setCurrentTerm(term, mID);

			System.out.println ("S" +
					mID +
					"." +
					term +
					": switched to candidate mode.");
			
			//Set timer that periodically checks if majority has voted for this candidate
			timer = startTimer();
			
			RaftResponses.setTerm(term);
			RaftResponses.clearVotes(term);

			// Sends request votes RPC to all other servers in parallel
			for (int server_id = 0; server_id < mConfig.getNumServers(); server_id++){
				remoteRequestVote(server_id + 1, term, mID, mLog.getLastIndex(), mLog.getLastTerm());
			}

		}
	}
	
	private Timer startTimer() {
		synchronized (mLock) {
			if (timer != null)
				timer.cancel();
			// Timeouts (one random, one user set)
			int randomTime = ThreadLocalRandom.current().nextInt(ELECTION_TIMEOUT_MIN, ELECTION_TIMEOUT_MAX);
			int overrideTime = mConfig.getTimeoutOverride();

			// Prioritize validly set user timeouts over randomized timeout.
			timeout = (overrideTime <= 0) ? randomTime : overrideTime;
			timerID = mID;

			return super.scheduleTimer(timeout, timerID);
		}
	}

	/**
	 * Handles another server (perhaps itself) requesting a vote where this candidate is a candidate.
	 * Grants vote if not yet voted, the candidate log is at least as up-to-date, and
	 * the candidate term matches or exceeds. Denies vote if conditions are not met.
	 * 
	 * @param candidateTerm the current term of the candidate
	 * @param candidateID server ID of the candidate
	 * @param candidateLastLogIndex index of the candidate's last log entry
	 * @param candidateLastLogTerm term of the candidate's last log entry
	 * 
	 * @return 0 if vote granted, server's current term if vote denied
	 */
	public int requestVote (int candidateTerm, int candidateID, int candidateLastLogIndex, int candidateLastLogTerm) {
		synchronized (mLock) {
			int term = mConfig.getCurrentTerm ();
			
			// Vote for itself
			if (candidateID == mID)
				return 0;
			
			// According to Piazza @854, you only vote for the other candidate if their term is strictly greater.
			// If the candidate has a lower or equal term, do not vote for them.
			if (term >= candidateTerm)
				return term;
			
			startTimer();
			
			int lastLogTerm = mLog.getLastTerm();
			int lastLogIndex = mLog.getLastIndex();
			
			boolean qualified_candidate = false;
			if (lastLogTerm < candidateLastLogTerm) {
				qualified_candidate = true;
			} else if (lastLogTerm == candidateLastLogTerm && lastLogIndex <= candidateLastLogIndex) {
				qualified_candidate = true;
			}
			
			FollowerMode follower = new FollowerMode();
			
			if (!qualified_candidate) {
				mConfig.setCurrentTerm(candidateTerm, 0);
				RaftServerImpl.setMode(follower);
				return candidateTerm;
			}
			
			mConfig.setCurrentTerm(candidateTerm, candidateID);
			RaftServerImpl.setMode(follower);
			return 0;
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
			timer.cancel();
			RaftServerImpl.setMode(new FollowerMode());
			return 0;
		}
	}

	// If any of the votes return their term, we know we are unqualified to be leader and should step down.
	
	public void handleTimeout (int timerID) {
		synchronized (mLock) {
			if (timerID == mID) {
				int term = mConfig.getCurrentTerm();
				int [] votes = RaftResponses.getVotes(term);
				
				int votesForMe = 0;
				int max_term = Integer.MIN_VALUE;
				for (int vote : votes) {
					if  (vote > term) {
						max_term = Math.max(max_term, vote);
					} else if (vote == 0) {
						votesForMe++;
					}
				}
				if (max_term > term) {
					mConfig.setCurrentTerm(max_term, 0);
					timer.cancel();
					RaftServerImpl.setMode(new FollowerMode());
					return;
				}
				if (votesForMe * 2 > mConfig.getNumServers()) {
					timer.cancel();
					RaftServerImpl.setMode(new LeaderMode());
				} else {
					timer.cancel();
					this.go();
				}
			}
		}
	}
}
