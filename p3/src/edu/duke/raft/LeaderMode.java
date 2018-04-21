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
      for (int server_id = 1; server_id < numServer + 1; server_id++){
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
    synchronized (mLock) {
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
    //for each follower
    for (int server_id = 1; server_id < mConfig.getNumServers() + 1; server_id++){
      if (server_id != mID){

        if (mLog.getLastIndex() < nextIndex[server_id]){
          //send empty append entries RPC
          remoteAppendEntries(server_id, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm(), new Entry[0], mCommitIndex);
        }else{
          //send log entries starting at nextIndex
          Entry[] logEntries = new Entry[mLog.getLastIndex() - nextIndex[server_id] + 1]
          for (int i = 0; i < logEntries.length; i++){
            logEntries[i] = mLog.getEntry(i + nextIndex[server_id]);
          }
          remoteAppendEntries(server_id, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm(), logEntries, mCommitIndex);
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
  public int requestVote (int candidateTerm,
  int candidateID,
  int lastLogIndex,
  int lastLogTerm) {
    synchronized (mLock) {

      if (candidateTerm > mConfig.getCurrentTerm ()){
        //if candidate's term is larger than current

        if (lastLogTerm >= mLog.getLastTerm() || (lastLogTerm == mLog.getLastTerm() && lastLogIndex >= mLog.getLastIndex()) ){
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
      if (leaderID == mID){
        return -1;
      }

      else if (leaderTerm < mConfig.getCurrentTerm()){
        return mConfig.getCurrentTerm();
      }

      else if (leaderTerm > mConfig.getCurrentTerm()){
        heartbeatTimer.cancel();
        //revert to follower
        RaftServerImpl.setMode(new FollowerMode());
        mConfig.setCurrentTerm(candidateTerm, leaderID);
      }

      else if (leaderTerm == mConfig.getCurrentTerm()){
        if (prevLogTerm >= mLog.getLastTerm()){
          heartbeatTimer.cancel();
          RaftServerImpl.setMode(new FollowerMode());
          return -1;
        }

    }
    return -1;
  }

  /*
  * send heartbeat messages if timeout
  *
  *  @param id of the timer that timed out
  */
  public void handleTimeout (int timerID) {
    synchronized (mLock) {
      /*heartbeat timer timed out:
      * send requestAppendEntries empty to all followers
      * start new heartbeatTimer
      */
      if (timerID == heartbeatTimerID) {

        //check if the append entries rpc reponse has a term higher than leader
        int[] appendResponses = RaftResponse.getAppendResponses(mConfig.getCurrentTerm());
        for (int i = 1; i < appendResponses +1; i++){
          //if response is 0, server appended entries
          //otherwise, server's curent term, append RPC failed
          if (appendResponses[i] <= 0){
            break;
          }
          if (appendResponses[i] > 0){
            int followerTerm = appendResponses[i];
            //if follwer term is larger than current term/leader term
            if (followerTerm > mConfig.currentTerm()){

              //stop this heartbeat timer
              heartbeatTimer.cancel();

              //update current term to the higher term
              mConfig.setCurrentTerm(followerTerm);

              //leader convert into follower, this term ends, start new election
              RaftServerImpl.setMode(new FollowerMode());
              return;
            }
          }
        }


        //send appendRPC periodicaly here
        sendAppendEntriesRPC();

        //start new heartbeatTimer
        heartbeatTimer = startHeartBeatTimer();

      }
    }
  }
}
