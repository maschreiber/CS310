package edu.duke.raft;

import java.util.Timer;
import java.util.concurrent.ThreadLocalRandom;

/*
 * Leader's tasks:
 * 1) Send periodic heartbeat to prevent timeout
 * 2) command received from client: append entry to local log, respond after entry applied to state machine
 * 3) If last log index > nextIndex for a follower send AppendEntries RPC with log entries
 * 4) If response contains term T > currentTerm, set currentTerm = T, covert to follower
 */

public class LeaderMode extends RaftMode {

	private Timer heartbeatTimer;
	private int heartbeatTimeout;
	private int heartbeatTimerID;
	private int[] nextIndex;
	private int[] matchIndex;


	public void go () {
		synchronized (mLock) {
			int term = mConfig.getCurrentTerm();
			RaftResponses.setTerm(term);
			RaftResponses.clearAppendResponses(mConfig.getCurrentTerm());

			System.out.println ("S" +
					mID +
					"." +
					term +
					": switched to leader mode.");

			//set heartbeat timer
			heartbeatTimer = startHeartBeatTimer();

			int numServer = mConfig.getNumServers();



			nextIndex = new int[numServer + 1];
			matchIndex = new int[numServer + 1];
			for (int server_id = 1; server_id < numServer +1; server_id++){
				//initialize nextIndex to leader last log index + 1
				nextIndex[server_id] = mLog.getLastIndex() + 1;
				//initialize matchIndex to 0
				matchIndex[server_id] = 0;
			}
			//nextindex: for each server, index of the next log entry to send to that server
			//matchindex: for each server index of highest log entry known to be replicated on server

			//send initial empty AppendEntries RPCs to each server
			sendAppendEntriesRPC();


		}
	}

	private Timer startHeartBeatTimer() {
		synchronized (mLock) { // CHECK need lock?
			if (heartbeatTimer != null)
				heartbeatTimer.cancel();
			// Timeouts (one heartbeat, one user set)
			int heartbeatTime = HEARTBEAT_INTERVAL;
			int overrideTime = mConfig.getTimeoutOverride();

			// Prioritize validly set user timeouts over randomized timeout.
			heartbeatTimeout = (overrideTime <= 0) ? heartbeatTime : overrideTime;
			heartbeatTimerID = mID * 7;

			return super.scheduleTimer(heartbeatTimeout, heartbeatTimerID);
		}
	}

	/*
	 * send append entries RPC to followers after each timeout
	 * If last log index ≥ nextIndex for a follower: send AppendEntries RPC with log entries starting at nextIndex
	 * otherwise send empty entry
	 */
	public void sendAppendEntriesRPC(){
		// CHECK if conditions
		synchronized (mLock) {
			//for each follower
			for (int server_id = 1; server_id < mConfig.getNumServers() + 1; server_id++){
				if (server_id != mID){

					if (mLog.getLastIndex() < nextIndex[server_id]){
						//send empty append entries RPC
						remoteAppendEntries(server_id, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm(), new Entry[0], mCommitIndex);
					}else{
						//send log entries starting at nextIndex
						Entry[] logEntries = new Entry[mLog.getLastIndex() - nextIndex[server_id] + 1];
						for (int i = 0; i < logEntries.length; i++){
							logEntries[i] = mLog.getEntry(i + nextIndex[server_id]);
						}
						remoteAppendEntries(server_id, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm(), logEntries, mCommitIndex);
					}

				}
			}
		}
	}



	// @param candidate’s term
	// @param candidate requesting vote
	// @param index of candidate’s last log entry
	// @param term of candidate’s last log entry
	// @return 0, if server votes for candidate; otherwise, server's
	// current term
	
	//will vote for candidate if at least as up to date as theirs
	public int requestVote (int candidateTerm,
			int candidateID,
			int lastLogIndex,
			int lastLogTerm) {
		synchronized (mLock) {

			if (candidateTerm > mConfig.getCurrentTerm ()){
				//if candidate's term is larger than current

				if (lastLogTerm >= mLog.getLastTerm()){
					heartbeatTimer.cancel();
					mConfig.setCurrentTerm(candidateTerm, candidateID);
					RaftServerImpl.setMode(new FollowerMode());
					return 0;
				}else{
					heartbeatTimer.cancel();
					mConfig.setCurrentTerm(candidateTerm, mID);
					RaftServerImpl.setMode(new FollowerMode());
					return mConfig.getCurrentTerm ();
				}

			}
			
			if (candidateTerm == mConfig.getCurrentTerm ()){

				if (lastLogTerm >= mLog.getLastTerm()){
					heartbeatTimer.cancel();
					RaftServerImpl.setMode(new FollowerMode()); //how to solve the first come first server voting mechanism
					return 0;
				}

			}
			

			return mConfig.getCurrentTerm ();
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
			
			//there can only be one leader in one term
			//CHECK question what should append entries return in this case?
			if (leaderTerm > mConfig.getCurrentTerm()) {
				heartbeatTimer.cancel();
				mConfig.setCurrentTerm(leaderTerm, 0);
				RaftServerImpl.setMode(new FollowerMode());
			}
			
			return mConfig.getCurrentTerm();
		}
	}

	/*
	 * send heartbeat messages if timeout
	 *
	 *  @param id of the timer that timed out
	 */
	public void handleTimeout (int timerID) {
		synchronized (mLock) {
			/*heartbeat timer timed out:
			 * 1) check responses from each timer cycle, update nextIndex, if should revert to follower etc
			 * 2) start new heartbeatTimer
			 * 3) Send new appendRPC = heartbeat + append Log
			 */
			if (timerID == heartbeatTimerID) {
				//check this cycle's AppendEntries RPC responses
				appendEntriesResponse();
				
				//send new AppendEntries RPC
				sendAppendEntriesRPC();
				
			}
		}
	}
	
	/*
	 * after sending AppendEntries RPC with log entries starting at nextIndex:
	 * Responses stored in RaftResponses.getAppendResponses(mConfig.getCurrentTerm())
	 * If successful -> 0: update nextIndex (next log entry index) and matchIndex (highest log entry) for follower
	 * If fails ->  because of log inconsistency: decrement nextIndex and retry
	 */
	
	public void appendEntriesResponse() {
		
		
		
		//array of AppendEntries RPC responses  
		int[] followerResponses = RaftResponses.getAppendResponses(mConfig.getCurrentTerm());
		
		//if maxFollowerTerm is larger than current in the end, revert to follower
		int maxFollowerTerm = mConfig.getCurrentTerm();
		
		//for each follower response
		for (int i = 1; i < followerResponses.length; i++) {
			//if follower responses term is larger than the max term
			if (followerResponses[i] > maxFollowerTerm) {
				maxFollowerTerm = followerResponses[i];
			}
			//if successful update nextIndex
			//if fails decrement nextIndex and retry
			if (followerResponses[i] != 0) {
				nextIndex[i] -= 1; //this will be checked/retried in method sendAppendEntriesRPC()
			}
	
		}
		
		//if there exists a follower with higher term than current term, leader revert to follower
		if (maxFollowerTerm > mConfig.getCurrentTerm()) {
			heartbeatTimer.cancel();
			mConfig.setCurrentTerm(maxFollowerTerm, 0);
			RaftServerImpl.setMode(new FollowerMode());
		}else {
			//if leader stays as leader, initiate new cycle
			heartbeatTimer.cancel();
			heartbeatTimer = scheduleTimer(heartbeatTimeout, heartbeatTimerID);
			RaftResponses.clearAppendResponses(mConfig.getCurrentTerm());	
		}
		
	}
	
	
	
	
	
	
	
	
	
	
}
