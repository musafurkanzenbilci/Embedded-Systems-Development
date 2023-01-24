// Code your design here
// Code your design here
// Code your design here
`timescale 1ns / 1ps

module ROM (
  	input [2:0] addr, 
	output reg [7:0] dataOut);
  reg [7:0] memory[7:0] ;
	// write your code below
  initial begin
    memory[0]=8'b00000000;
    
    memory[1]=8'b01010101;
    
    memory[2]=8'b10101010;
    memory[3]=8'b00110011;
    
    memory[4]=8'b11001100;
    
    memory[5]=8'b00001111;
    memory[6]=8'b11110000;
    
    memory[7]=8'b11111111;
  end
  always@(*) begin
    dataOut<=memory[addr];
  end
	
	
endmodule


module XOR_RAM (
input mode, 
input [2:0] addr, 
input [7:0] dataIn, 
input [7:0] mask, 
input CLK, 
output reg [7:0] dataOut);
	reg [7:0] memory[7:0] ;
	// write your code below
  initial begin
    memory[0]=8'b00000000;
    
    memory[1]=8'b00000000;
    
    memory[2]=8'b00000000;
    memory[3]=8'b00000000;
    
    memory[4]=8'b00000000;
    
    memory[5]=8'b00000000;
    memory[6]=8'b00000000;
    
    memory[7]=8'b00000000;
    dataOut = 8'b00000000;
  end
  always@(*) begin
    if(mode==1)
    	dataOut<=memory[addr];
    
    
  end
  always@(posedge CLK) begin
    if(mode==0)
      memory[addr] <= dataIn^mask;
  end
	// write your code below
	
endmodule


module EncodedMemory (
input mode, 
input [2:0] index, 
input [7:0] number, 
input CLK, 
output [7:0] result);
	
	// DO NOT EDIT THIS MODULE
	
	wire [7:0] mask;
	
	ROM R(index, mask);
	XOR_RAM XR(mode, index, number, mask, CLK, result);

endmodule
