import java.util.Random;
import java.util.List;
import java.util.LinkedList;

public class LBSim
{
  private final static int SLEEP_TIME = 1;
  private final static int NUM_NODES = 32;
  private final static int MAX_ACTIVE = 3;
  private final static int FISH_DELAY = 250;
  private final static int FRAME_DURATION = 1024;
  private final static int TRANSMIT_TIME = 100;
  private NodeThread[] nodes;
  private int totalFish = 0;
  private int totalSchedule = 0;
  private int framesRemaining = 0;

  class Message
  {
    int arrivalTime;
  }

  class FishMessage extends Message
  {
    NodeThread requester;
    int ttl;
  }

  class ScheduleMessage extends Message
  {
    private List<Frame> frames = new LinkedList<Frame>();
  }

  class NodeStats
  {
    public int sparks;
    public int runnable;

    public NodeStats(int sparks, int runnable)
    {
      this.sparks = sparks;
      this.runnable = runnable;
    }
  }

  class Frame
  {
    public int duration;

    public Frame(int duration)
    {
      this.duration = duration;
    }
  }

  class NodeThread extends Thread
  {
    private int index;
    private List<Frame> sparks = new LinkedList<Frame>();
    private List<Frame> runnable = new LinkedList<Frame>();
    private Random rand = new Random(hashCode());
    private List<Message> mailbox = new LinkedList<Message>();
    private volatile int iterations = 0;
    private int lastFishTime = 0;

    public NodeThread(int index)
    {
      this.index = index;
    }

    public void postMessage(Message m)
    {
      synchronized (LBSim.this) {
        if (m instanceof FishMessage)
          totalFish++;
        else if (m instanceof ScheduleMessage)
          totalSchedule++;
      }
      synchronized (mailbox) {
        m.arrivalTime = iterations + TRANSMIT_TIME;
        mailbox.add(m);
      }
    }

    public Message pollMessage()
    {
      synchronized (mailbox) {
        if (!mailbox.isEmpty() && (mailbox.get(0).arrivalTime <= iterations))
          return mailbox.remove(0);
        else
          return null;
      }
    }

    public NodeStats reportStats()
    {
      synchronized (this) {
        return new NodeStats(sparks.size(),runnable.size());
      }
    }

    public void run()
    {
      synchronized (this) {
        while (true) {
//           if ((rand.nextDouble() >= 0.99) && (runnable > 0)) {
//             // Generate a new spark
//             sparks++;
//           }

//           if ((rand.nextDouble() >= 0.9999) && (runnable > 0)) {
//             // - Complete an existing call (starting a new one if necessary)
//             runnable--;
//           }

          Message m;
          while ((m = pollMessage()) != null) {
            if (m instanceof ScheduleMessage) {
              sparks.addAll(((ScheduleMessage)m).frames);
            }
            else if (m instanceof FishMessage) {
              FishMessage fm = (FishMessage)m;
              if (!sparks.isEmpty()) {
                ScheduleMessage sm = new ScheduleMessage();
                int toSchedule = Math.min(sparks.size(),128);
                for (int i = 0; i < toSchedule; i++)
                  sm.frames.add(sparks.remove(0));
                debug(index+": got fish from "+fm.requester.index+
                      ", scheduling "+toSchedule+" frames");
                fm.requester.postMessage(sm);
              }
              else {
                fm.ttl--;
                if (fm.ttl > 0) {
                  int dest = rand.nextInt(nodes.length);
                  debug(index+": got fish from "+fm.requester.index+
                        ", forwarding to "+dest+" (ttl "+fm.ttl+")");
                  nodes[dest].postMessage(fm);
                }
              }
            }
          }

          // Remove any completed frames
          int i = 0;
          while (i < runnable.size()) {
            Frame cur = runnable.get(i);
            cur.duration--;
            if (cur.duration <= 0) {
              runnable.remove(i);
              synchronized (LBSim.this) {
                framesRemaining--;
              }
//               debug(index+": frame completed");
            }
            else {
              i++;
            }
          }

          // Active sparks if we're below the threshold
          while ((runnable.size() < MAX_ACTIVE) && !sparks.isEmpty()) {
            runnable.add(sparks.remove(0));
          }

          // Send out FISH message if appropriate
          if (runnable.isEmpty() && sparks.isEmpty() &&
              (iterations - lastFishTime >= FISH_DELAY)) {
            lastFishTime = iterations;
            debug(index+": sending fish");
            FishMessage fm = new FishMessage();
            fm.requester = this;
            fm.ttl = nodes.length;
            nodes[rand.nextInt(nodes.length)].postMessage(fm);
          }

          // Delay
          try {
            wait(SLEEP_TIME);
          }
          catch (InterruptedException e) {}
          iterations++;
        }
      }
    }
  }

  public void debug(String msg)
  {
    synchronized (System.out) {
//       System.out.println(msg);
    }
  }

  public void run()
  {
    System.out.format("FRAMES FISH SCHEDULE UTIL\n"); 
    nodes = new NodeThread[NUM_NODES];
    for (int i = 0; i < nodes.length; i++)
      nodes[i] = new NodeThread(i);
    for (int i = 0; i < 4096; i++) {
      nodes[0].sparks.add(new Frame(FRAME_DURATION));
      framesRemaining++;
    }
    for (int i = 0; i < nodes.length; i++)
      nodes[i].start();
    while (true) {
      NodeStats[] stats = new NodeStats[nodes.length];
//       int totalFrames = 0;
      int utilisation = 0;

      for (int i = 0; i < nodes.length; i++) {
        stats[i] = nodes[i].reportStats();
//         totalFrames += stats[i].sparks + stats[i].runnable;
        if (stats[i].runnable == 3)
          utilisation++;
      }

      System.out.format("%-4d -- ",framesRemaining);
      synchronized (this) {
        System.out.format("%-4d %-4d %3d%% -- ",
                          totalFish,totalSchedule,100*utilisation/nodes.length);
      }
      for (int i = 0; i < nodes.length; i++) {
//         System.out.format("%-4d/%-4d ",stats[i].sparks,stats[i].runnable);
        System.out.format("%-4d ",stats[i].sparks);
      }
      System.out.format("\n");

      try {
        Thread.sleep(1000);
      }
      catch (InterruptedException e) {}
    }
  }

  public static void main(String[] args)
  {
    LBSim sim = new LBSim();
    sim.run();
  }
}
