package edu.duke.raft;

import java.util.Timer;
import java.util.concurrent.ThreadLocalRandom;

public class FollowerMode extends RaftMode {

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
					": switched to follower mode.");
			timer = startTimer();
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
			int lastLogTerm = mLog.getLastTerm();
			int votedFor = mConfig.getVotedFor();

			// If this server has already voted for another candidate, deny vote.
			if (votedFor != 0 && votedFor != candidateID)
				return term;

			// This server shouldn't have a higher term than the candidate.
			if (term > candidateTerm)
				return term;

			// This server's term of the last log entry is higher than the candidate's last log entry.
			if (lastLogTerm > candidateLastLogTerm) {
				mConfig.setCurrentTerm(candidateTerm, 0);
				return term;
			}
			
			mConfig.setCurrentTerm(candidateTerm, candidateID);
			startTimer();
			return 0;
		}
	}

	/**
	 * Append entries to local log.
	 * 
	 * @param leaaderTerm the current term of the leader
	 * @param leaedrID server ID of the leader
	 * @param leaderPrevLogIndex index of the log entry before entries to append
	 * @param leaderPrevLogTerm term of log entry before entries to append
	 * @param entries to append
	 * 
	 * @return 0 if successful append, server current term if append denied, -1 if log corrupted
	 */
	public int appendEntries (int leaderTerm, int leaderID, int leaderPrevLogIndex, 
			int leaderPrevLogTerm, Entry[] entries, int leaderCommit) {
		synchronized (mLock) {
			int term = mConfig.getCurrentTerm ();
			
			// Will not accept entries to append from a leader with a lower term.
			if (leaderTerm < term)
				return term;
			
			mConfig.setCurrentTerm(leaderTerm, 0);
			
			startTimer();
			Entry entry;
			try {
				entry = mLog.getEntry(leaderPrevLogIndex);
				if (entry == null)
					return -1;
				else if (entry.term != leaderPrevLogTerm)
					return -1;
			} catch (IndexOutOfBoundsException e) {
				return -1;
			}
			
			int insertCode = mLog.insert(entries, leaderPrevLogIndex, leaderPrevLogTerm);
			if (insertCode == -1)
				return -1;
			
			if (leaderCommit > mCommitIndex)
				mCommitIndex = Math.min(leaderCommit, insertCode);
			
			return 0;
		}
	}

	/**
	 * If timer finishes, promote self to candidate.
	 * 
	 * @param timerID recognizes the timer associated with this mode (follower) and this server
	 */
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
