package sim;

import java.awt.event.MouseAdapter;
import java.awt.event.MouseWheelEvent;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.PriorityQueue;

// please note that adjusting hyperparameters (eg. train speed, citizen spawn rate) drastically affects the simulation (yeah, I know, duh)

/* Potential extensions:
 * - better ways to customize hyperparameters
 * - custom per-line headways/frequencies and speeds
 * - train delays
 * - ability to click on trains (or citizens?) to see their paths
 * 		this could prove very computationally expensive, don't want to check every citizen and train but also don't want to update segments
 * - multithreading
 * - zoom to mouse & scaling panning to zoom
 * - map rotation
 * - background geography
 * - simulation statistics (+ graphing?)
 * - time-based pathfinding (not just distance-based, but using train arrival times)
 * - trains visually follow ComplexLine paths (this has been moved to another branch!)
 */

public class Simulator {

	public static void main(String [] args) {

		// app initialization
		Vector3 worldSize = new Vector3(150, 180, 15);
		Vector2 xyScaling = new Vector2(0.9, 1);
		Vector2 windowTopLeftCornerInPixels = new Vector2(64, 64);
		Vector3 backgroundColor = Vector3.white;
		int windowHeightInPixels = 1024;
		App app = new Sim(Sim.DEFAULT_INITIAL_SPEED, Sim.DEFAULT_SIM_SPEED_BOUNDS, worldSize, xyScaling,windowTopLeftCornerInPixels , backgroundColor, windowHeightInPixels);

		app.run();

	}

}

class Sim extends App {

	public static final Vector3 DEFAULT_SIM_SPEED_BOUNDS = new Vector3(0.2, 0.0, 10.0);
	public static final int DEFAULT_CITIZEN_ALLOCATION = 1024;
	public static final int DEFAULT_TRAIN_ALLOCATION = 8;
	public static final double DEFAULT_INITIAL_SPEED = 5;

	private Line[] lines;
	private Node[] nodes;
	private ArrayList<ArrayList<ArrayList<Drawable>>> segmentedNodes;
	private ComplexLine[] complexLines;
	private int numStops;
	private int nodeSegmentSize;
	private int ridershipTotal;

	private ArrayList<Citizen> citizens;
	
	private boolean paused;
	private int drawTrains;
	private int drawCitizens;
	private boolean drawTip;
	private boolean spawnCitizens;

	private double globalTime;
	private double citizenSpawnCycleTime;
	private double timeIncrement;
	private double tempTimeIncrement;
	private double TIME_INCREMENT_INCREMENT;
	private double MIN_TIME_INCREMENT;
	private double MAX_TIME_INCREMENT;

	private double MAP_X_SCALE;
	private double MAP_Y_SCALE;

	private Vector2 textPos;
	private Vector2 text2Pos;
	private Vector2 text3Pos;
	private Vector2 text4Pos;
	private Vector2 mouseInitialPos;
	private Node mousePosNode;

	public Sim(double timeIncrement, Vector3 INCREMENT_SETTINGS, Vector3 WORLD_SIZE, Vector2 MAP_X_Y_SCALE, Vector2 windowTopLeft, Vector3 backgroundColor, int windowHeight) {

		assert WORLD_SIZE.x % WORLD_SIZE.z <= 0.0001 && WORLD_SIZE.y % WORLD_SIZE.z <= 0.0001;

		this.timeIncrement = timeIncrement;
		this.tempTimeIncrement = timeIncrement;
		this.TIME_INCREMENT_INCREMENT = INCREMENT_SETTINGS.x;
		this.MIN_TIME_INCREMENT = INCREMENT_SETTINGS.y;
		this.MAX_TIME_INCREMENT = INCREMENT_SETTINGS.z;
		setWindowSizeInWorldUnits(WORLD_SIZE.x, WORLD_SIZE.y);
		this.nodeSegmentSize = (int) WORLD_SIZE.z;
		this.MAP_X_SCALE = MAP_X_Y_SCALE.x * 0.5;
		this.MAP_Y_SCALE = MAP_X_Y_SCALE.y * 0.5;
		setWindowTopLeftCornerInPixels((int) windowTopLeft.x, (int) windowTopLeft.y);
		setWindowCenterInWorldUnits(0.0, 0.0);
		setWindowHeightInPixels(windowHeight);
		setWindowBackgroundColor(backgroundColor);
		this._jFrame.setTitle("CitySim");

	}

	void setup() {

		Logger.log("Started setup");
				
		this.paused = false;
		this.drawTrains = 0;
		this.drawCitizens = 0;
		this.drawTip = true;
		this.spawnCitizens = true;
		
		// mouse wheel zooming
		this.addMouseWheelListener( new MouseAdapter() {
			@Override public void mouseWheelMoved(MouseWheelEvent e) {

				Drawable.adjustZoom(-e.getWheelRotation() * Drawable.ZOOM_CONST);

			}
		});
		
		textPos = new Vector2(-this._windowWidthInWorldUnits / 2, this._windowHeightInWorldUnits / 2 - 2);
		text2Pos = textPos.plus(new Vector2(0, -2));
		text3Pos = text2Pos.plus(new Vector2(0, -2));
		text4Pos = text3Pos.plus(new Vector2(0, -2));

		// initialize zooming and panning variables
		mouseInitialPos = new Vector2(0, 0);
		Drawable.resetPanZoom();
		mousePosNode = new Node(new Vector2(), Vector3.black, -1);

		// iterate through stations and add stops to appropriate lines
		ArrayList<Node> nodesList = new ArrayList<Node>();
		ArrayList<Double> stationXList = new ArrayList<Double>();
		ArrayList<Double> stationYList = new ArrayList<Double>();

		HashMap<String, Line> lines = new HashMap<String, Line>();
		ridershipTotal = 0;
		numStops = 0;
		
		int c = 0;
		
		try (InputStream in = getClass().getResourceAsStream("stations_data.csv")) {

			BufferedReader reader = new BufferedReader(new InputStreamReader(in));
			String line;

			while ((line = reader.readLine()) != null) {
				
				numStops++;
				String[] n = line.split(",");				
				String[] stopLines = n[4].split("-");

				for (String str : stopLines) {

					if (!lines.containsKey(str)) {

						lines.put(str, new Line(str));

					}

				}

				int ridership =  Integer.parseInt(n[5]);
				ridershipTotal += ridership;
				Node stop = new Node(n[1], new Vector2(), Vector3.black,ridership);
				nodesList.add(stop);
				stationXList.add(Double.parseDouble(n[2]));
				stationYList.add(Double.parseDouble(n[3]));
				c++;

				// this is rather slow
				for (String str : stopLines) {

					lines.get(str).addStop(stop, 1);

				}

			}

		} catch (IOException e) { Logger.log("Could not load station data"); e.printStackTrace(); assert false; }
		
		nodes = new Node[numStops];
		double[] stationX = new double[numStops];
		double[] stationY = new double[numStops];
		
		for (int i = 0; i < numStops; i++) {
			
			nodes[i] = nodesList.get(i);
			stationX[i] = stationXList.get(i);
			stationY[i] = stationYList.get(i);
			
		}
		
		Logger.log("Read " + nodes.length + " station nodes");

		// convert real-world geometry data to world units
		Vector2 xMinMax = getMinMax(stationX);
		Vector2 yMinMax = getMinMax(stationY);
		normalize(stationX, -this._windowWidthInWorldUnits * MAP_X_SCALE, this._windowWidthInWorldUnits * MAP_X_SCALE, xMinMax.x, xMinMax.y);
		normalize(stationY, -this._windowHeightInWorldUnits * MAP_Y_SCALE, this._windowHeightInWorldUnits * MAP_Y_SCALE, yMinMax.x, yMinMax.y);

		for (int i = 0; i < stationX.length; i++) {

			nodes[i].setPos(stationX[i], stationY[i]);

		}

		// used for easiest n-nearest detection throughout
		segmentedNodes = new ArrayList<ArrayList<ArrayList<Drawable>>>();

		for (int i = 0; i < (int) this._windowWidthInWorldUnits / nodeSegmentSize + 3; i++) {

			segmentedNodes.add(new ArrayList<ArrayList<Drawable>>());
			for (int j = 0; j < (int) this._windowHeightInWorldUnits / nodeSegmentSize + 3; j++) {

				segmentedNodes.get(i).add(new ArrayList<Drawable>());

			}

		}

		for (Node n : nodes) { 

			// this code should probably be moved to a function
			int xIndex = ((int) (this._windowWidthInWorldUnits/2 + n.getPos().x)/nodeSegmentSize) + 1;
			int yIndex = ((int) (this._windowHeightInWorldUnits/2 + n.getPos().y)/nodeSegmentSize) + 1;

			segmentedNodes.get(xIndex).get(yIndex).add(n);
			n.setSegmentIndex(xIndex, yIndex);

		}

		// generate nearby neighbors for walking transfers
		for (Node n : nodes) {

			n.addNearbyNeighbors(segmentedNodes);

		}
		
		Logger.log("Segmented nodes into " + segmentedNodes.size() + " by " + segmentedNodes.get(0).size() + " grid and processd position data.");

		// load in configurations for proper stop orders for lines
		HashMap<String, String> lineConfigs = new HashMap<String, String>();
		try (InputStream in = getClass().getResourceAsStream("lines_stations.csv")) {

			BufferedReader reader = new BufferedReader(new InputStreamReader(in));
			String line;
			while ((line = reader.readLine()) != null) {

				if (line.indexOf(",") == -1) { continue; }
				String lineID = line.substring(0, line.indexOf(","));
				lines.get(lineID).setColor(new Vector3Mod(line.substring(line.indexOf(",") + 1, line.indexOf(",", line.indexOf(",") + 1))));
				lineConfigs.put(line.substring(0, line.indexOf(",")), line.substring(line.indexOf(",", line.indexOf(",") + 1) + 1));

			}

		} catch (IOException e) { Logger.log("Could not load line data"); assert false; }

		// apply line configurations
		c = 0;
		for (Line l : lines.values()) {

			l.rearrangeStops(lineConfigs.get(l.getID()).split(","));
			l.overrideStopColors();
			
			Train[] trains = new Train[l.getLength() / Line.DEFAULT_TRAIN_SPAWN_SPACING + 1];
			for (int i = 0; i < trains.length; i++) {

				trains[i] = new Train(c+"", this, (i * Line.DEFAULT_TRAIN_SPAWN_SPACING) % l.getLength(), l, l.getColor(), Train.DEFAULT_TRAIN_SPEED);
				c++;

			
			l.setTrains(trains);
			
			}

		}

		Logger.log("Loaded and applied line configurations for lines " + lines.keySet());

		// add lines to simulation array
		c = 0;
		this.lines = new Line[lines.keySet().size()];

		for (Line l : lines.values()) {

			this.lines[c++] = l;

		}

		// generate complex lines for drawing
		ArrayList<ComplexLine> complexLinesBuilder = new ArrayList<ComplexLine>();
		try (InputStream in = getClass().getResourceAsStream("lines_geom_data.csv")) {

			BufferedReader reader = new BufferedReader(new InputStreamReader(in));
			String line;

			while ((line = reader.readLine()) != null) {

				String[] lineData = line.split(",");
				Vector3 lineColor = Vector3.black;
				if (lines.containsKey(lineData[4])) {

					lineColor = lines.get(lineData[4]).getColor();

				}
				String[] lineX = lineData[6].split(" ");
				String[] lineY = lineData[7].split(" ");
				double[] lineXD = new double[lineX.length];
				double[] lineYD = new double[lineY.length];
				Node[] lineNodes = new Node[lineX.length];

				for (int i = 0; i < lineX.length; i++) {

					lineXD[i] = Double.parseDouble(lineX[i]);
					lineYD[i] = Double.parseDouble(lineY[i]);

				}

				normalize(lineXD, -this._windowWidthInWorldUnits * MAP_X_SCALE, this._windowWidthInWorldUnits * MAP_X_SCALE, xMinMax.x, xMinMax.y);
				normalize(lineYD, -this._windowHeightInWorldUnits * MAP_Y_SCALE, this._windowHeightInWorldUnits * MAP_Y_SCALE, yMinMax.x, yMinMax.y);

				for (int i = 0; i < lineXD.length; i++) {

					lineNodes[i] = new Node(new Vector2(lineXD[i], lineYD[i]), lineColor, -1);

				}

				complexLinesBuilder.add(new ComplexLine(lineData[1], lineColor, lineNodes, Double.parseDouble(lineData[5])));

			}

		} catch (IOException e) { Logger.log("Could not load ComplexLine data"); assert false; }

		complexLines = new ComplexLine[complexLinesBuilder.size()];
		for (int i = 0; i < complexLines.length; i++) {

			complexLines[i] = complexLinesBuilder.get(i);

		}
		
		for (ComplexLine n : complexLines) { 

			int xIndex1 = ((int) (this._windowWidthInWorldUnits/2 + n.getHead().getPos().x)/nodeSegmentSize) + 1;
			int yIndex1 = ((int) (this._windowHeightInWorldUnits/2 + n.getHead().getPos().y)/nodeSegmentSize) + 1;
			int xIndex2 = ((int) (this._windowWidthInWorldUnits/2 + n.getTail().getPos().x)/nodeSegmentSize) + 1;
			int yIndex2 = ((int) (this._windowHeightInWorldUnits/2 + n.getTail().getPos().y)/nodeSegmentSize) + 1;
			
			segmentedNodes.get(xIndex1).get(yIndex1).add(n);
			segmentedNodes.get(xIndex2).get(yIndex2).add(n);
			n.setSegmentIndex(xIndex1, yIndex1);

		}
		
		Logger.log("Loaded and generated " + complexLines.length + " ComplexLine objects and segmented into grid");

		// build and populate citizen arraylist
		citizens = new ArrayList<Citizen>(Sim.DEFAULT_CITIZEN_ALLOCATION);
		this.citizenSpawnCycleTime = 0;
		
		for (int i = 0; i < Citizen.INITIAL_SPAWN_AMOUNT; i++) {
			
			citizens.add(new Citizen(this, Node.findPath(sample(nodes, ridershipTotal), sample(nodes, ridershipTotal))));
			
		}
				
		Logger.log("Populated " + Citizen.INITIAL_SPAWN_AMOUNT + " citizens");
		
		Logger.log("Starting simulation!");
		Logger.disable();
				
	}

	void loop() {

		this.globalTime += timeIncrement;
		this.citizenSpawnCycleTime += timeIncrement;
		
		// get nearest node to mouse and calculate necessary position for manual citizen spawning
		Vector2 realMousePos = mousePosition.times(1/Drawable.getZoom()).minus(Drawable.getMousePan()).minus(Drawable.getPan());
		int mouseXSegment = (int)Drawable.constrict(((int) (this._windowWidthInWorldUnits/2 + realMousePos.x)/nodeSegmentSize) + 1, 1, segmentedNodes.size()-2);
		int mouseYSegment = (int)Drawable.constrict(((int) (this._windowHeightInWorldUnits/2 + realMousePos.y)/nodeSegmentSize) + 1, 1, segmentedNodes.size()-2);
		mousePosNode.setPos(realMousePos);
		mousePosNode.setSegmentIndex(mouseXSegment, mouseYSegment);
		mousePosNode.clearNeighbors();
		mousePosNode.addNearbyNeighbors(segmentedNodes, false, Citizen.CLICK_SPAWN_MAX_DIST);
		
		Node nearestNeighbor = null;
		if (mousePosNode.getNeighbors().size() == 0) {
			
			double dist = Drawable.distanceBetween(mousePosNode, nodes[0]);
			for (int i = 1; i < nodes.length; i++) {
				
				double dist2 = Drawable.distanceBetween(mousePosNode, nodes[i]);
				if (dist2 < dist) {
					
					dist = dist2;
					nearestNeighbor = nodes[i];
					
				}
				
			}
			
		} else {
			
			double dist = Double.MAX_VALUE;
			for (Node.PathWrapper pw : mousePosNode.getNeighbors().keySet()) {
				
				Node n = pw.getNode();
				double dist2 = Drawable.distanceBetween(mousePosNode, n);
				if (dist2 < dist) {
					
					dist = dist2;
					nearestNeighbor = n;
					
				}
				
			}
			
		}

		// panning and zooming with mouse and keyboard
		if (mousePressed) {

			mouseInitialPos = new Vector2(mousePosition).minus(Drawable.getMousePan());

		} if (mouseHeld) {

			Drawable.setMousePan(mousePosition.minus(mouseInitialPos));

		}

		if (keyHeld('A')) {

			Drawable.adjustPan(-Drawable.PAN_CONST, 0);

		} if (keyHeld('D')) {

			Drawable.adjustPan(Drawable.PAN_CONST, 0);

		} if (keyHeld('W')) {

			Drawable.adjustPan(0, Drawable.PAN_CONST);

		} if (keyHeld('S')) {

			Drawable.adjustPan(0, -Drawable.PAN_CONST);

		}

		if (keyHeld('=')) {

			Drawable.adjustZoom(Drawable.ZOOM_CONST);

		} if (keyHeld('-')) {

			Drawable.adjustZoom(-Drawable.ZOOM_CONST);

		}

		if (keyPressed('C')) {

			Drawable.resetPanZoom();

		}

		// simulation speed adjustments
		if (keyPressed('1')) {

			this.timeIncrement = Drawable.constrict(timeIncrement-TIME_INCREMENT_INCREMENT, MIN_TIME_INCREMENT, MAX_TIME_INCREMENT);

		} if (keyPressed('2')) {

			this.timeIncrement = Drawable.constrict(timeIncrement+TIME_INCREMENT_INCREMENT, MIN_TIME_INCREMENT, MAX_TIME_INCREMENT);

		} if (keyPressed('P')) {
			
			if (paused) {
				
				this.timeIncrement = this.tempTimeIncrement;
				
			} else {
				
				this.tempTimeIncrement = timeIncrement;
				this.timeIncrement = 0;
				
			}
			
			paused = !paused;
			
		}
		
		// drawing adjustments
		if (keyPressed('T')) {
			
			drawTrains++;
			drawTrains %= 3;
			
		}
		
		if (keyPressed('G')) {
			
			drawCitizens++;
			drawCitizens %= 3;
			
		}
		
		if (keyPressed('I')) {
			
			drawTip = !drawTip;
			
		}
		
		// citizen adjustments
		if (keyPressed('F')) {
			
			spawnCitizens = !spawnCitizens;
			
		} if (keyPressed('\b')) {
			
			citizens = new ArrayList<Citizen>(DEFAULT_CITIZEN_ALLOCATION);
			for (Line l : lines) {
				
				for (Train t : l.getTrains()) {
					
					t.clearCitizens();
					
				}
				
			}
			
			for (Node n : nodes) {
				
				n.clearCitizens();
				
			}
			
		}
		
		// manually spawn citizens
		if (keyPressed('E')) {
			
			Node from = nearestNeighbor;
			int max = Citizen.SPAWN_MAX;
			if (Citizen.SPAWN_RANDRANGE) { max *= Math.random(); }
			Node[] neighboringNodes = mousePosNode.getNeighboringNodes();
			for (int i = 0; i < max; i++) {
				
				if (mousePosNode.getNeighbors().size() != 0) { from = sample(neighboringNodes); }
				citizens.add(new Citizen(this, Node.generateWalkingPath(mousePosNode, from, sample(nodes, ridershipTotal))));
				
			}
			
			Logger.log("User spawned in nodes near " + nearestNeighbor);
			
		}
		
		// despawn citizens
		// amount of citizens can vary with simulation speed, not fully intentional
		int c1 = 0; int c2 = 0;
		if (this.citizenSpawnCycleTime >= Citizen.SPAWN_INTERVAL) {
			
			this.citizenSpawnCycleTime = 0;
			for (int i = 0; i < citizens.size(); i++) {
				
				if (citizens.get(i).getGlobalTime() >= Citizen.MAX_TIME_ALIVE) {
					
					c1++;
					citizens.get(i).getCurrentNode().removeCitizen(); // this could cause excessive removals (but no citizens ever seem to get garbage collected, so it's fine)
					citizens.get(i).removeFromTrain();
					citizens.remove(i);
					continue;
					
				}
				
				if (citizens.get(i).getStatus().equals(TransitStatus.DESPAWN)) {
					
					c2++;
					citizens.remove(i);
					
				}
				
			}
			
			Logger.log("Despawned " + (c1+c2) + " citizens, " + c1 + " garbage collected and " + c2 + " natural");
			
			if (spawnCitizens) {
				
				int max = Citizen.SPAWN_MAX;
				if (Citizen.SPAWN_RANDRANGE) { max *= Math.random(); }
				for (int i = 0; i < max; i++) {
					
					Node from = sample(nodes, ridershipTotal);
					double xRange = Math.random() * Citizen.SPAWN_MAX_DIST - Citizen.SPAWN_MAX_DIST/2;
					double yRange = Math.random() * Citizen.SPAWN_MAX_DIST - Citizen.SPAWN_MAX_DIST/2;
					Node randPos = new Node(from.getPos().plus(new Vector2(xRange, yRange)), Vector3.black, i);
					randPos.setSize(0);
					citizens.add(new Citizen(this, Node.generateWalkingPath(randPos, from, sample(nodes, ridershipTotal))));
					
				}
				
				Logger.log("Spawned " + max + " citizens");
				
			}
			
		}
		
		// draw game objects
		for (ComplexLine cl : complexLines) {

			Drawable.drawComplexLine(this, cl);

		}

		for (Node n : nodes) {

			Drawable.drawCircle(this, n);

		}
		
		for (Line l : lines) {

			for (Train t : l.getTrains()) {

				t.updatePosAlongLine();
				if (drawTrains % 3 != 2) {
					
					if (drawTrains % 3 == 1 && t.getCitizens() >= Train.SMALL_CITIZENS || drawTrains %3 == 0) {
						
						Drawable.drawCircle(this, t);
						Drawable.drawString(this, t, t.getLine().getID(), Vector3.black, Train.FONT_SIZE, Train.FONT_CENTERED);
						
					}

					
				}

			}

		}
		
		for (Citizen c : citizens) {

			c.followPath();
			// checking for walking speeds up draw time significantly, but may hinder debug efforts
			if (drawCitizens % 3 != 0) {
				
				if ( (c.getStatus().equals(TransitStatus.WALKING) || c.getStatus().equals(TransitStatus.LINE_TRANSFER)) && drawTrains % 3 == 1 && c.getColor().equals(Citizen.DEFAULT_CITIZEN_COLOR) || drawCitizens % 3 == 2 && c.getColor().equals(Citizen.DEFAULT_CITIZEN_SPAWN_COLOR)) {
					
					Drawable.drawCircle(this, c);

				}
				
			}

		}
		
		String simSpeed = String.format("%.3f", this.timeIncrement);
		if (this.timeIncrement == 0) { simSpeed = "PAUSED"; }
		drawString("Current simulation speed: " + simSpeed, this.textPos, Vector3.black, 12, false);
		drawString("Ae mctive citizen agents: " + citizens.size(), this.text2Pos, Vector3.black, 12, false);
		String nearestNeighborString = "NULL";
		if (nearestNeighbor != null) { nearestNeighborString = nearestNeighbor.getID() + ", " + nearestNeighbor.getCitizens() + " passengers waiting"; }
		drawString("Nearest station: " + nearestNeighborString, this.text3Pos, Vector3.black, 12, false);
		if (drawTip) { drawString("1/2/P sim speed, T/G show trains/citizens, C/R reset view/sim, E/F/bkspc control citizen spawning, I to hide this", this.text4Pos, Vector3.black, 12, false); }

	}
	
	// some utility methods used in setup/loop
	public static Vector2 getMinMax(double[] arr) {

		if (arr == null || arr.length == 0) { return null; }

		double minInArr = arr[0];
		double maxInArr = arr[0];
		for (double i : arr) {

			if (i < minInArr) { minInArr = i; }
			if (i > maxInArr) { maxInArr = i; }

		}

		return new Vector2(minInArr, maxInArr);

	}

	public static boolean normalize(double[] arr, double min, double max, double minInArr, double maxInArr) {

		double maxMinDiff = max - min;
		double maxMinInArrDiff = maxInArr - minInArr;

		if (maxMinInArrDiff + min == 0) { return false; }

		for (int i = 0; i < arr.length; i++) {

			arr[i] = maxMinDiff * (arr[i] - minInArr) / (maxMinInArrDiff) + min;

		}

		return true;

	}
	
	public static Node sample(Node[] nodes, int sum) {
		
		int rand = (int)(Math.random() * sum);
		int randCount = 0;
		for (Node n : nodes) {
			
			if (randCount >= rand) {
				
				return n;
				
			}
			randCount += n.getRidership();
			
		}
		
		return sample(nodes);
		
	}
	
	public static Node sample(Node[] nodes) {
		
		return nodes[(int)(Math.random() * nodes.length)];
		
	}

	public double getTimeIncrement() { return this.timeIncrement; }
	public double getGlobalTime() { return this.globalTime; }
	public String toString() { return "City simulation running for " + globalTime + " ticks."; }
	
	static class Logger {
		
		private static long time = System.currentTimeMillis();
		private static boolean enabled = true;
		
		public static void log(String str) {
			
			if (!enabled) { return; }
			System.out.println(String.format("%.3f", (System.currentTimeMillis() - time) / 1000.0) + "ms: " + str);
			
		}
		
		public static void enable() { enabled = true; }
		public static void disable() { enabled = false; }
		public static boolean isEnabled() { return enabled; }
		
	}

}

class Drawable {

	final static double ZOOM_CONST = 0.05;
	final static double ZOOM_MAX = 4;
	final static double ZOOM_MIN = 0.1;
	final static double PAN_CONST = 0.25;
	final static double PAN_X_MINMAX = 50;
	final static double PAN_Y_MINMAX = 50;

	private static double zoom;
	private static Vector2 pan;
	private static Vector2 mousePan;

	private String id;
	private Vector2 pos;
	private Vector3 color;
	private double size;
	
	private int xSegmentIndex;
	private int ySegmentIndex;

	public Drawable(Vector2 pos, Vector3 color, double size) {

		this("", pos, color, size);

	}

	public Drawable(String id, Vector2 pos, Vector3 color, double size) {

		this.id = id;
		this.pos = pos;
		this.color = color;
		this.size = size;

	}
	
	public static double constrict(double d, double min, double max) {

		return Math.min(Math.max(d, min), max);

	}

	public static void adjustPan(double x, double y) {

		pan = pan.minus(new Vector2(x, y));
		constrictPan(pan);

	}

	public static void constrictPan(Vector2 pan) {

		pan.x = constrict(pan.x, -PAN_X_MINMAX, PAN_X_MINMAX);
		pan.y = constrict(pan.y, -PAN_Y_MINMAX, PAN_Y_MINMAX);

	}

	public static void adjustZoom(double z) {

		zoom = Math.min(Math.max(zoom+z, ZOOM_MIN), ZOOM_MAX);

	}
	
	public static double distanceBetween(Drawable d1, Drawable d2) {
		
		return Vector2.distanceBetween(d1.getPos(), d2.getPos());
		
	}

	public static void drawCircle(App a, Drawable d) {

		drawCircle(a, d, d.getColor());

	}
	
	public static void drawCircle(App a, Drawable d, Vector3 col) {

		a.drawCircle(d.getPos().plus(pan).plus(mousePan).times(zoom), d.getSize() * zoom, col);

	}

	public static void drawLine(App a, Drawable d1, Drawable d2) {

		drawLine(a, d1, d2, d2.getColor());

	}

	public static void drawLine(App a, Drawable d1, Drawable d2, Vector3 col) {

		a.drawLine(d1.getPos().plus(pan).plus(mousePan).times(zoom), d2.getPos().plus(pan).plus(mousePan).times(zoom), col);

	}

	public static void drawComplexLine(App a, ComplexLine cl) {

		drawComplexLine(a, cl, cl.getColor());

	}

	public static void drawComplexLine(App a, ComplexLine cl, Vector3 col) {

		for (int i = 1; i < cl.getNodesSize(); i++) {

			drawLine(a, cl.getNode(i-1), cl.getNode(i), col);

		}

	}

	public static void drawPath(App a, ArrayList<Node.PathWrapper> path) {

		drawCircle(a, path.get(0).getNode());
		for (int i = 1; i < path.size(); i++) {

			drawCircle(a, path.get(i).getNode(), Vector3.red);
			drawLine(a, path.get(i-1).getNode(), path.get(i).getNode(), Vector3.red);

		}

	}

	public static void drawString(App a, Drawable d, String str, int size, boolean centered) {

		drawString(a, d, str, d.getColor(), size, centered);

	}

	public static void drawString(App a, Drawable d, String str, Vector3 col, int size, boolean centered) {

		a.drawString(str, d.pos.plus(pan).plus(mousePan).times(zoom), col, (int) Math.ceil(size * zoom), centered);

	}

	public static void resetPanZoom() { pan = new Vector2(); mousePan = new Vector2(); zoom = 1; }
	public static void setPan(Vector2 p) { constrictPan(p); pan = p; }
	public static void setMousePan(Vector2 mp) { constrictPan(mp); mousePan = mp; }
	public static void setZoom(double z) { zoom = z; }
	public static Vector2 getPan() { return pan; }
	public static Vector2 getMousePan() { return mousePan; }
	public static double getZoom() { return zoom; }

	public void setID(String id) { this.id = id; }
	public void setPos(Drawable d) { this.pos = d.getPos(); }
	public void setPos(Vector2 pos) { this.pos = pos; }
	public void setPos(double x, double y) { this.pos.x = x; this.pos.y = y; }
	public void setX(double x) { this.pos.x = x; }
	public void setY(double y) { this.pos.y = y; }
	public void setSegmentIndex(int x, int y) { this.xSegmentIndex = x; this.ySegmentIndex = y; }
	public void setColor(Vector3 col) { this.color = col; }
	public void setSize(double size) { this.size = size; }
	public String getID() { return this.id; }
	public Vector2 getPos() { return this.pos; }
	public double getX() { return this.pos.x; }
	public double getY() { return this.pos.y; }
	public int getXSegmentIndex() { return this.xSegmentIndex; }
	public int getYSegmentIndex() { return this.ySegmentIndex; }
	public Vector3 getColor() { return this.color; }
	public double getSize() { return this.size; }
	public String toString() { return "Drawable id=" + this.id + " pos=" + this.pos; }

}

class CitizenContainer extends Drawable {

	public static final double MAX_SIZE = Node.DEFAULT_NODE_SIZE * 5;
	
	private int numCitizens;
	
	public CitizenContainer(String id, Vector2 pos, Vector3 color, double size) {
	
		super(id, pos, color, size);
		this.numCitizens = 0;
	
	}
	
	public void addCitizen() { this.numCitizens++; }
	public void removeCitizen() { this.numCitizens = Math.max(0, --this.numCitizens); }
	public void clearCitizens() { this.numCitizens = 0; }
	public int getCitizens() { return this.numCitizens; }
	public double getSize() { return Math.min(MAX_SIZE, this.numCitizens * Citizen.DEFAULT_CONTAINER_CITIZEN_SIZE + super.getSize()); }
	
}

class Node extends CitizenContainer {

	public static final double DEFAULT_NODE_SIZE = 0.5;
	public static final double DEFAULT_TRANSFER_WEIGHT = 16;
	public static final double DEFAULT_TRANSFER_MAX_DIST = 2;
	public static final double DEFAULT_CONST_STOP_PENALTY = 2;
	public static final double DEFAULT_CONST_TRANSFER_PENALTY = 24;

	private HashMap<PathWrapper, Double> neighbors;
	private HashMap<String, Train> currentTrains;

	// ridership of -1 indicates that it is not a station
	private double ridership;
	private double score;

	public Node(Vector2 pos, Vector3 color, int ridership) {

		this("", pos, color, ridership);

	}

	public Node(String id, Vector2 pos, Vector3 color, int ridership) {

		super(id, pos, color, DEFAULT_NODE_SIZE);
		clear();
		this.score = 0; this.ridership = ridership;

	}

	public void addNeighbor(Node n, double d, Line l) {

		if (n == this) { return; }
		neighbors.put(new PathWrapper(n, l), d);

	}
	
	public void addNearbyNeighbors(ArrayList<ArrayList<ArrayList<Drawable>>> segmentedNodes) {
		
		addNearbyNeighbors(segmentedNodes, true, Node.DEFAULT_TRANSFER_MAX_DIST);
		
	}
	
	public void addNearbyNeighbors(ArrayList<ArrayList<ArrayList<Drawable>>> segmentedNodes, boolean addToBoth, double range) {
		
		for (int x = -1; x <= 1; x++) {
			
			for (int y = -1; y <= 1; y++) {
				
				for (Drawable n : segmentedNodes.get(getXSegmentIndex()+x).get(getYSegmentIndex()+y)) {
					
					if (!n.getClass().equals(Node.class)) { continue; }
					Node nNode = (Node) n;
					double dist = Drawable.distanceBetween(this, n);
					if (nNode.isStation() && dist <= range) {
						
						if (addToBoth) {
							
							addNeighborPair(this, nNode, dist, Line.WALKING_LINE);
							
						} else {
							
							this.addNeighbor(nNode, dist, Line.WALKING_LINE);
							
						}
						
					}
					
				}
				
			}
			
		}
		
	}

	public static void addNeighborPair(Node a, Node b, double dist, Line l) {

		a.addNeighbor(b, dist, l);
		b.addNeighbor(a, dist, l);

	}

	// pathfinding
	public double getScore() { return this.score; }
	public void setScore(double score) { this.score = score; }

	// line-based path generation may still not be 100% working as intended
	public static ArrayList<PathWrapper> findPath(Node start, Node end) {

		PriorityQueue<Node> queue = new PriorityQueue<>(Comparator.comparingDouble(Node::getScore));
		HashSet<Node> visited = new HashSet<Node>();
		HashMap<Node, PathWrapper> from = new HashMap<Node, PathWrapper>();
		HashMap<Node, Double> score = new HashMap<Node, Double>();

		score.put(start, 0.0);
		start.setScore(score.get(start) + Drawable.distanceBetween(start, end));
		queue.add(start);

		while (!queue.isEmpty()) {

			Node current = queue.poll();

			// path found
			if (current.equals(end)) {

				ArrayList<PathWrapper> path = reconstructPath(from, end);
				if (path.size() > 0) { path.add(new PathWrapper(end, path.get(path.size()-1).getLine())); }
				return path;

			}

			visited.add(current);

			for (PathWrapper pathWrapper : current.getNeighbors().keySet()) {

				Node neighbor = pathWrapper.getNode();
				Line line = pathWrapper.getLine();
				
				if (visited.contains(neighbor)) { continue; }

				double aggregateScore = score.get(current) + current.getNeighbors().get(pathWrapper);

				// anti-transfer heuristic
				if (from.get(neighbor) != null && !from.get(neighbor).getLine().equals(line)) {

					aggregateScore += Node.DEFAULT_CONST_TRANSFER_PENALTY;

				}

				if (!queue.contains(neighbor) || aggregateScore < score.get(neighbor)) {

					from.put(neighbor, new PathWrapper(current, line));
					score.put(neighbor, aggregateScore);
					neighbor.setScore(score.get(neighbor) + Drawable.distanceBetween(neighbor, end));

					if (!queue.contains(neighbor)) {

						queue.add(neighbor);

					}

				}

			}
		}

		return null; // no path

	}

	private static ArrayList<PathWrapper> reconstructPath(HashMap<Node, PathWrapper> from, Node current) {

		ArrayList<PathWrapper> path = new ArrayList<PathWrapper>();

		while (from.containsKey(current)) {

			PathWrapper currentPW = from.get(current);
			current = currentPW.getNode();
			path.add(new PathWrapper(current, currentPW.getLine()));

		}

		for (int i = 0; i < path.size()/2; i++) {

			int j = path.size()-1-i;
			PathWrapper temp = path.get(i);
			path.set(i, path.get(j));
			path.set(j, temp);

		}

		return path;

	};
	
	public static ArrayList<PathWrapper> generateWalkingPath(Node spawn, Node start, Node end) {
		
		ArrayList<PathWrapper> path = findPath(start, end);
		path.add(0, new PathWrapper(spawn, Line.WALKING_LINE));
		return path;
		
	}

	public void addTrain(Train train) { if (!this.currentTrains.containsKey(train.getID())) { this.currentTrains.put(train.getID(), train); } }
	public void removeTrain(Train train) { this.currentTrains.remove(train.getID()); }
	public void clear() { clearNeighbors(); clearTrains(); }
	public void clearNeighbors() { this.neighbors = new HashMap<PathWrapper, Double>(); }
	public void clearTrains() { this.currentTrains = new HashMap<String, Train>(Sim.DEFAULT_TRAIN_ALLOCATION); }
	public void setRidership(double d) { this.ridership = d; }
	public HashMap<PathWrapper, Double> getNeighbors() { return this.neighbors; }
	public Node[] getNeighboringNodes() { Node[] nodes = new Node[this.getNeighbors().size()]; int i = 0; for (PathWrapper pw : this.getNeighbors().keySet()) { nodes[i++] = pw.getNode(); } return nodes; }
	public HashMap<String, Train> getCurrentTrains() { return this.currentTrains; }
	public double getRidership() { return this.ridership; }
	public boolean isStation() { return this.ridership >= 0; }
	public String toString() { return "Node id=" + this.getID() + " pos=" + this.getPos(); }

	static class PathWrapper {

		private Node node;
		private Line line;

		public PathWrapper(Node node, Line line) {

			this.node = node;
			this.line = line;

		}

		public Node getNode() { return this.node; }
		public Line getLine() { return this.line; }
		public String toString() { return "PathWrapper line=" + line.getID() + " node=" + node; }

	}

}

class ComplexLine extends Drawable {

	private Node[] nodes;
	private Node head;
	private Node tail;
	private double length;

	public ComplexLine(String id, Vector3 col, Node[] nodes, double length) {

		super(id, nodes[0].getPos(), col, -1);
		this.head = nodes[0];
		this.tail = nodes[nodes.length-1];
		this.nodes = nodes;
		this.length = length;

	}

	public void setNodes(Node[] nodes) { this.nodes = nodes; }
	public void setLength(double length) { this.length = length; }
	public Node[] getNodes() { return this.nodes; }
	public Node getHead() { return this.head; }
	public Node getTail() { return this.tail; }
	public Node getNode(int i) { return this.nodes[i]; }
	public int getNodesSize() { return nodes.length; }
	public double getLength() { return length; }
	public String toString() { return "ComplexLine id=" + this.getID() + " numNodes=" + this.getNodesSize() + " length=" + this.getLength(); }

}

enum TransitStatus {

	WALKING, LINE_TRANSFER, WAITING_AT_STATION, ON_TRAIN, SPAWN, DESPAWN

}

class Citizen extends Drawable {

	public static final Vector3 DEFAULT_CITIZEN_SPAWN_COLOR = Vector3.gray;
	public static final Vector3 DEFAULT_CITIZEN_COLOR = Vector3.black;
	public static final double DEFAULT_CITIZEN_SIZE = 0.25;
	public static final double DEFAULT_CONTAINER_CITIZEN_SIZE = (CitizenContainer.MAX_SIZE - Train.DEFAULT_TRAIN_SIZE) / Train.DEFAULT_TRAIN_CAPACITY;
	public static final double DEFAULT_UNLOAD_TIME = 16;
	public static final double DEFAULT_CITIZEN_SPEED = 0.02;
	public static final double MAX_TIME_ALIVE = 2048;
	public static final double SPAWN_MAX_DIST = 10;
	public static final double CLICK_SPAWN_MAX_DIST = 10;
	public static final double SPAWN_INTERVAL = 4;
	public static final int INITIAL_SPAWN_AMOUNT = 1024;
	public static final int SPAWN_MAX = 128;
	public static final int CLICK_SPAWN_MAX = 128;
	public static final boolean SPAWN_RANDRANGE = true;

	private Sim sim;
	private TransitStatus status;
	private Node currentNode;
	private Node nextNode;
	private Train currentTrain;
	private Line currentLine;
	private Line nextLine;
	private Node.PathWrapper[] path;
	private int pathIndex;
	private boolean justBoarded;

	private double globalTime;
	private double actionTime;
	private double walkTime;
	private double walkDist;
	private Vector2 initialWalkPos;
	private double speed;

	public Citizen(Sim sim, ArrayList<Node.PathWrapper> path) {

		super(new Vector2(), Citizen.DEFAULT_CITIZEN_SPAWN_COLOR, Citizen.DEFAULT_CITIZEN_SIZE);
		this.sim = sim;
		
		if (path == null || path.size() <= 1) {
			
			this.status = TransitStatus.DESPAWN;
			return;
			
		}
		
		setPos(path.get(0).getNode());
		this.status = TransitStatus.SPAWN;
		this.pathIndex = 0;
		this.globalTime = 0;
		this.actionTime = 0;
		this.walkTime = 0;
		this.speed = Citizen.DEFAULT_CITIZEN_SPEED;
		justBoarded = false;
		
		
		this.path = new Node.PathWrapper[path.size()];
		for (int i = 0; i < this.path.length; i++) {

			this.path[i] = path.get(i);

		}

	}
	
	// just so you know, this function is a pretty horrible mess
	public void followPath() {
				
		if (status == TransitStatus.DESPAWN) { return; }

		double modSpeed = speed * sim.getTimeIncrement();
		globalTime += sim.getTimeIncrement();
		actionTime += sim.getTimeIncrement();

		if (pathIndex == path.length) {

			this.getCurrentNode().removeCitizen();
			despawn();
			return;

		}

		nextNode = path[pathIndex].getNode();
		nextLine = path[pathIndex].getLine();

		switch (this.status) {

		case WALKING:
			// start walking along path
			if (walkTime == 0) {
				
				walkDist = Drawable.distanceBetween(this, nextNode);
				initialWalkPos = this.getPos();
				
			}
			
			if (walkTime >= walkDist) {
				
				// at station, ready to proceed to next path node
				walkTime = 0;
				moveAlongPath();
				if (currentLine == Line.WALKING_LINE) {

					status = TransitStatus.WALKING;

				} else {

					status = TransitStatus.WAITING_AT_STATION;
					this.currentNode.addCitizen();
					this.setColor(DEFAULT_CITIZEN_COLOR);
					
				}

			} else {

				// continue walk to next path node
				setPos(Vector2.lerp(walkTime/walkDist, initialWalkPos, nextNode.getPos()));
				walkTime += modSpeed;
				
			}

			break;
		case LINE_TRANSFER:
			if (actionTime >= DEFAULT_UNLOAD_TIME) {

				// ready to wait for train
				actionTime = 0;
				status = TransitStatus.WAITING_AT_STATION;
				
			}
			
			break;
		case WAITING_AT_STATION:
			setPos(this.currentNode);
			for (Train t : this.currentNode.getCurrentTrains().values()) {

				if (t.getRealNextStop().equals(nextNode) && t.getCitizens() <= Train.DEFAULT_TRAIN_CAPACITY) {

					// path-following is greedy if citizens will take any available train to next stop
					if (Train.GREEDY_PATH && !t.getLine().equals(currentLine)) { continue; }
					
					// ready to board train
					this.currentNode.removeCitizen();
					currentTrain = t;
					this.currentTrain.addCitizen();
					moveAlongPath();
					status = TransitStatus.ON_TRAIN;
					justBoarded = true;
					break;

				}

			}
			
			break;
		case ON_TRAIN:
			// circumvents counting the stop that a citizen just got on the train at as part of their journey
			if (justBoarded && currentTrain.getStatus().equals(TransitStatus.ON_TRAIN)) { justBoarded = false; }
			if (!justBoarded && currentTrain.getStatus().equals(TransitStatus.WAITING_AT_STATION) && currentTrain.getStop().equals(currentNode)) {

				// how on earth is this not executing too many times?? whatever
				moveAlongPath();
				
				if (!currentLine.equals(currentTrain.getLine())) {

					// at station and transfer required, ready to proceed to next path node
					removeFromTrain();
					if (currentLine.equals(Line.WALKING_LINE)) {
						
						status = TransitStatus.WALKING;
						
					} else {
						
						status = TransitStatus.LINE_TRANSFER;
						currentNode.addCitizen();
						
					}
					
					setPos(currentNode);
					
					currentTrain = null;
					actionTime = 0;
						
				}

			} else {

				// move with train
				setPos(currentTrain);

			}
			
			break;
		case SPAWN:
			if (nextLine.equals(Line.WALKING_LINE)) {

				status = TransitStatus.WALKING;

			} else {

				this.setColor(DEFAULT_CITIZEN_COLOR);
				status = TransitStatus.WAITING_AT_STATION;
				moveAlongPath();
				this.currentNode.addCitizen();
				
				
			}
			
			break;
		default:
			break;
			
		}

	}
	
	private void despawn() {
		
		// assumes that node removals have already been handled correctly, this may be resulting in excess visual accumulation across nodes
		removeFromTrain();
		this.status = TransitStatus.DESPAWN;
		
	}

	private void moveAlongPath() {

		actionTime = 0;
		currentNode = nextNode;
		currentLine = nextLine;
		pathIndex++;
		if (pathIndex == path.length) {
			
			despawn();
			return;
			
		}
		nextNode = path[pathIndex].getNode();
		nextLine = path[pathIndex].getLine();

	}
	
	public void removeFromTrain() {
		
		if (this.currentTrain != null) { this.currentTrain.removeCitizen(); }
		
	}

	public void setStatus(TransitStatus status) { this.status = status; }
	public void setNode(Node node) { this.currentNode = node; }
	public void setTrain(Train train) { this.currentTrain = train; }
	public void setLine(Line line) { this.currentLine = line; }
	public void setPath(Node.PathWrapper[] path) { assert path != null & path.length >= 1; this.path = path; this.nextNode = path[0].getNode(); this.nextLine = path[0].getLine(); }
	public double getGlobalTime() { return this.globalTime; }
	public double getActionTime() { return this.actionTime; }
	public TransitStatus getStatus() { return this.status; }
	public Node getCurrentNode() { return this.currentNode; }
	public Train getCurrentTrain() { return this.currentTrain; }
	public Line getCurrentLine() { return this.currentLine; }
	public Node.PathWrapper[] getPath() { return this.path; }
	public Node.PathWrapper getCurrentPathStep() { return this.path[this.pathIndex]; }
	public int getPathIndex() { return this.pathIndex; }
	public String toString() { return "Citizen id=" + getID() + " pos=" + getPos() + " pathStep=" + getCurrentPathStep(); }

}

class Train extends CitizenContainer {

	public static final int DEFAULT_TRAIN_CAPACITY = 256;
	public static final double DEFAULT_TRAIN_SIZE = 1.0;
	public static final double DEFAULT_STOP_DURATION = 12;
	public static final double DEFAULT_TRAIN_SPEED = 0.08;
	public static final int FONT_SIZE = 11;
	public static final boolean FONT_CENTERED = true;
	public static final boolean GREEDY_PATH = true;
	public static final int SMALL_CITIZENS = 1;

	private TransitStatus status;
	private Sim sim;
	private Line line;
	private int stop;
	private int nextStop;
	private double globalTime;
	private double stopTime;
	private double stoppedTime;
	private double speed;

	public Train(String id, Sim sim, int spawnStop, Line line, Vector3 color, double speed) { 

		super(id, line.getStop(spawnStop).getPos(), color, DEFAULT_TRAIN_SIZE);
		this.sim = sim;
		this.line = line;
		this.stop = spawnStop;
		this.speed = speed;
		this.status = TransitStatus.SPAWN;

	}

	public void updatePosAlongLine() {

		double modSpeed = speed * sim.getTimeIncrement();
		nextStop = (this.stop+1) % line.getLength();
		globalTime += sim.getTimeIncrement();
		stopTime += modSpeed;

		if (stopTime >= this.line.getDist(nextStop)) {

			// go to next station
			if (stoppedTime >= DEFAULT_STOP_DURATION) {

				this.status = TransitStatus.ON_TRAIN;
				this.getNextStop().removeTrain(this);
				stopTime = 0;
				stoppedTime = 0;
				stop = nextStop;
				setPos(this.line.getStop(stop));

			// wait at station
			} else {

				this.status = TransitStatus.WAITING_AT_STATION;
				this.getNextStop().addTrain(this);
				stoppedTime += sim.getTimeIncrement();

			}

		} else {

			// move along line
			this.status = TransitStatus.ON_TRAIN;
			setPos(Vector2.lerp(stopTime/this.line.getDist(nextStop), this.line.getStop(stop).getPos(), this.line.getStop(nextStop).getPos()));

		}

	}

	public void setSpeed(double speed) { this.speed = speed; }
	public TransitStatus getStatus() { return this.status; }
	public Line getLine() { return this.line; }
	// current stop is the stop the train was last physically at, next stop is the stop that the train either is headed to or just arrived at, real next stop is a stop that the train has not arrived at yet
	// yes, this can be confusing
	public Node getStop() { return this.line.getStop(this.stop); }
	public Node getNextStop() { return this.line.getStop(this.nextStop); }
	public Node getRealNextStop() { return this.line.getStop((this.stop + 2) % this.line.getLength()); }
	public int getStopIndex() { return this.stop; }
	public double getGlobalTime() { return this.globalTime; }
	public double getStopTime() { return this.stopTime; }
	public double getStoppedTime() { return this.stoppedTime; }
	public double getSpeed() { return this.speed; }
	public String toString() { return "Train id=" + this.getID() + " Line=" + this.getLine().getID() + " pos=" + this.getPos(); }

}

class Line {

	public static final int DEFAULT_TRAIN_SPAWN_SPACING = 3;
	public static final Line WALKING_LINE = new Line("Transfer");

	private String id;
	private Vector3 color;

	private Node[] stops;
	private double[] dists;
	private Train[] trains;

	public Line(String id) {

		this.id = id;

	}

	public void addStop(Node stop, double dist) {

		if (this.stops == null) { this.stops = new Node[0]; }
		if (this.dists == null) { this.dists = new double[0]; }
		Node[] newStops = new Node[this.stops.length + 1];
		double[] newDists = new double[this.stops.length + 1];
		assert this.stops.length == this.dists.length;

		for (int i = 0; i < this.stops.length; i++) {

			newStops[i] = this.stops[i];
			newDists[i] = this.dists[i];

		}

		newStops[newStops.length-1] = stop;
		newDists[newDists.length-1] = dist;

		this.stops = newStops;
		this.dists = newDists;

	}

	// reconfigure stop list according to preset stop pattern
	// this is one of the wonkier features of the simulation setup process
	public void rearrangeStops(String[] stopConfig) {

		ArrayList<Node> newStops = new ArrayList<Node>();
		for (String s : stopConfig) {

			for (Node n : this.stops) {

				if (n.getID().equals(s.replaceAll("_", " "))) {

					newStops.add(n);

				}

			}

		}

		int size = newStops.size();
		for (int i = 1; i < size; i++) {

			newStops.add(newStops.get(size-1-i));

		}

		dists = new double[newStops.size()];
		for (int i = 0; i < newStops.size()-1; i++) {

			double dist = Drawable.distanceBetween(newStops.get(i), newStops.get(i+1));
			dists[i+1] = dist;
			Node.addNeighborPair(newStops.get(i), newStops.get(i+1), dist + Node.DEFAULT_CONST_STOP_PENALTY, this);

		}

		stops = new Node[newStops.size()];
		for (int i = 0; i < stops.length; i++) {

			stops[i] = newStops.get(i);

		}

	}

	// set stop colors to line color
	public void overrideStopColors() {

		for (int i = 0; i < this.stops.length; i++) {

			this.stops[i].setColor(color);

		}

	}

	public void setStops(Node[] stops, double[] dists) { this.stops = stops; this.dists = dists; }
	public void setTrains(Train[] trains) { this.trains = trains; }
	public void setColor(Vector3 col) { this.color = col; }
	public void setID(String id) { this.id = id; }
	public Node[] getStops() { return this.stops; }
	public double[] getDists() { return this.dists; }
	public Node getStop(int i) { return this.stops[i]; }
	public double getDist(int i) { return this.dists[i]; }
	public int getLength() { return this.stops.length; }
	public Train[] getTrains() { return this.trains; }
	public Vector3 getColor() { return this.color; }
	public String getID() { return this.id; }
	public String toString() { return "Line id= " + this.id; }

}

class Vector3Mod extends Vector3 {

	Vector3Mod(String hex) {

		this.x = (double)Integer.parseInt(hex.substring(0, 2), 16)/255.0;
		this.y = (double)Integer.parseInt(hex.substring(2, 4), 16)/255.0;
		this.z = (double)Integer.parseInt(hex.substring(4, 6), 16)/255.0;

	}

}
