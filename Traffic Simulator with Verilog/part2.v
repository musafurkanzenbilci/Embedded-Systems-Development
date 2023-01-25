
`timescale 1ns / 1ps
module IntersectionSimulator(
	input [2:0] mode, //1xx:display, 000:remA, 001:remB, 010:addA, 011:addB
	input [4:0] plateIn,
	input action,
	input clk,
	output reg  greenForA,
	output reg  greenForB,
	output reg 	rushHourWarning,
	output reg [3:0]  hour,
	output reg [5:0]  minute,	
	output reg [5:0]  second,
	output reg	am_pm, //0:am, 1:pm
	output reg [4:0]  numOfCarsA,
	output reg [4:0]  numOfCarsB,
	output reg [6:0]  remainingTime,
	output reg [4:0]  blackListDisplay
	);
	
  
  reg [6:0]  nextremainingTimeForA;
  reg [6:0]  nextremainingTimeForB;
  reg [6:0]  currentremainingTimeForA;
  reg [6:0]  currentremainingTimeForB;
  reg [6:0]  maxHourForA;
  reg [6:0]  maxHourForB;
  reg [6:0]  minHourForA;
  reg [6:0]  minHourForB;
  reg whichwasgreen;
  	//queue
  reg [4:0] plateListForA[30:0];
  reg [4:0] plateListForB[30:0];
  reg [4:0] blackList[30:0];
  reg [4:0] blackListLength;
  reg [4:0] lengthA;
  reg [4:0] lengthB;
  reg [4:0] plateHolder;
  reg [4:0] blackListIndex;
  integer i;
  reg delay;
  //Initialization
	initial begin
      	blackListIndex = 0;
      	delay=0;
		greenForA=1;
		greenForB=0;
		rushHourWarning=0;
		hour=6;
		minute=0;
		second=0;
		am_pm=0;
		numOfCarsA=0;
		numOfCarsB=0;
		remainingTime=50;
		blackListDisplay=0;
		
		//...
      whichwasgreen = 0;
      currentremainingTimeForB = 50;
      currentremainingTimeForA = 40;
	  nextremainingTimeForA = 40;
      nextremainingTimeForB = 55;
      //
      for(i=0;i<=30;i=i+1) begin
        plateListForA[i] = 5'b00000;
      end
      for(i=0;i<=30;i=i+1) begin
        plateListForB[i] = 5'b00000;
      end
      for(i=0;i<=30;i=i+1) begin
        blackList[i] = 5'b00000;
      end
      lengthA=0;
      lengthB = 0;
  	  blackListLength = 0;
      
     
	end




  
  
  
  //queue-like insert and remove operations
  //insert
  //plateListForA[lengthA] <= something;
  //lengthA <= lengthA+1;
  //remove
  //plateHolder <= plateListForA[0];
  //for(i=0;i<lengthA;i=i+1) begin
  //  plateListForA[i]<=plateListForA[i+1];
  //end
  //lengthA <= lengthA-1;  
    
    
    
	always@(posedge action)
	begin
	
		//...
      //Car remove or add
      //If car is removed in red light put it in the blacklist
     
        if(mode == 3'b000) begin
          //remove operation
          numOfCarsA=numOfCarsA-1;
          plateHolder = plateListForA[0];
  		  for(i=0;i<lengthA;i=i+1) begin
            plateListForA[i]=plateListForA[i+1];
  		  end
          lengthA = lengthA-1;
          if(!greenForA) begin
            //blacklist add
            blackList[blackListLength] = plateHolder;
  			blackListLength = blackListLength+1;
          end
          
        end
        else if(mode == 3'b001) begin
          numOfCarsB=numOfCarsB-1;
          plateHolder= plateListForB[0];
          for(i=0;i<lengthB;i=i+1) begin
            plateListForB[i]=plateListForB[i+1];
  		  end
          lengthB = lengthB-1;
          if(!greenForB) begin
            //blacklist add
            blackList[blackListLength] = plateHolder;
  			blackListLength = blackListLength+1;
          end
        end 
        else if(mode == 3'b010) begin
        	numOfCarsA=numOfCarsA+1;
          	plateListForA[lengthA] = plateIn;
  			lengthA = lengthA+1;
          	
        end
        else if(mode == 3'b011) begin
        	numOfCarsB=numOfCarsB+1;
          plateListForB[lengthB] = plateIn;
  		    lengthB = lengthB+1;
        end
      
	end

  
  
  always @(mode) begin
  	blackListIndex = 0;
  end

	always @(posedge clk)
	begin
      //Display Mode
      if(mode === 3'b100 || mode === 3'b111 || mode === 3'b110 || mode === 3'b101 || mode === 3'b1xx)begin
        if(blackListLength == 0) begin
        	blackListDisplay = 4'b1000;
        end
        else if (blackListLength == 1) begin
          	blackListDisplay = blackList[0];
        end
        else begin 
        	blackListDisplay = blackList[blackListIndex];
        	if(blackListIndex == blackListLength - 1) begin
        		blackListIndex = 0;
        	end else begin
        		blackListIndex = blackListIndex+1;
        	end
        end
        
       
      end else begin
      	//Time Flow
      remainingTime=remainingTime-1;
      if(second==59) begin
         second=0;
        if(minute==59) begin
         	minute=0;
          if(hour==12 ) begin
            hour=1;
            
            end
          else if (hour==11) begin
          	am_pm=~am_pm;
            hour=hour+1;
          end
          
          else begin
              hour=hour+1;
            end
          end else begin
          minute=minute+1;
          end
        end else begin 
      	second=second+1;
        end
      //BlackList Reset at Midnight
      if(!am_pm && hour==0 && minute==0 && second == 0) begin
        for(i=0;i<blackListLength;i=i+1) begin
          blackList[i] = 5'b00000;
        end
      end
      //Rush Hour Warning
      if((!am_pm && (hour==7 || hour==8)) || (am_pm && (hour==5 || hour == 6))) begin
        rushHourWarning=1;
      end else begin
      	rushHourWarning=0;
      end
      //What happens if rushHourWarning
      if(rushHourWarning) begin
        maxHourForA=60;
        minHourForA=30;
        maxHourForB=70;
        minHourForB=40;
      end else begin
      	maxHourForA=70;
        minHourForA=40;
        maxHourForB=80;
        minHourForB=50;
      end
      if(rushHourWarning) begin
        if(greenForA && remainingTime>maxHourForB) begin
        	remainingTime = maxHourForB;
        end
        if(greenForB && remainingTime>maxHourForA) begin
        	remainingTime = maxHourForA;
        end
      end
      // 1 SECOND DELAY
      if(delay) begin
        delay=0;
        if(whichwasgreen) begin
        	greenForA=1;
          	whichwasgreen= 0;
          	//B is red now
        	remainingTime = nextremainingTimeForB;
          //---------------------
          if(currentremainingTimeForB<=maxHourForB && currentremainingTimeForB>=minHourForB) begin
            if(numOfCarsB>=0 && numOfCarsB<=10) begin
              if(currentremainingTimeForB<=maxHourForB-5) begin
                
          		nextremainingTimeForB = currentremainingTimeForB+5;
              end else begin
              	nextremainingTimeForB =80;
              end
          	end
            if(numOfCarsA>=11 && numOfCarsA<=19) begin
          		nextremainingTimeForB = currentremainingTimeForB;
          	end
            if(numOfCarsA>=20) begin
              if(currentremainingTimeForB>=minHourForB+5) begin
                
          		nextremainingTimeForB = currentremainingTimeForB-5;
              end else begin
              	nextremainingTimeForB =50;
              end
          	end
          end
          	currentremainingTimeForB= remainingTime;
        	//----------------
        end else begin
        	greenForB=1;
          whichwasgreen = 1;
          //A is red now
        	remainingTime = nextremainingTimeForA;
          //-------------
          if(currentremainingTimeForA<=maxHourForA && currentremainingTimeForA>=minHourForA) begin
          	if(numOfCarsA>=0 && numOfCarsA<=10) begin
              if(currentremainingTimeForA<=maxHourForA-5) begin
                	
          		nextremainingTimeForA = currentremainingTimeForA+5;
                end else begin
                	nextremainingTimeForA =70;
                end
          	end
            if(numOfCarsA>=11 && numOfCarsA<=19) begin
          		nextremainingTimeForA = currentremainingTimeForA;
          	end
            if(numOfCarsA>=20) begin
              if(currentremainingTimeForA>=minHourForA+5) begin
                
          		nextremainingTimeForA = currentremainingTimeForA-5;
                end else begin
                  
          		nextremainingTimeForA = 40;
                end
          	end
          end
          	currentremainingTimeForA= remainingTime;
          //-------------
          
        //-------------------------
        end
      end
      
      //Next Red Light Duration
      if(remainingTime==0) begin
        delay=1;
        if(greenForA) begin
          
          greenForA=~greenForA;
        end
        if(greenForB) begin
          
          
        	greenForB=~greenForB; 
        end
      end
      
      
      end
      
	end

  
  

endmodule
