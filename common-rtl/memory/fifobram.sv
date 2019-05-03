`include "pipearch_common.vh"

module fifobram
#(
	parameter WIDTH = 8,
	parameter LOG2_DEPTH = 5
)
(
	input logic clk,
	input logic reset,
	fifobram_interface.source access
);

logic internal_reset;
always_ff @(posedge clk)
begin
    internal_reset <= reset;
end

logic [WIDTH-1:0] memory [2**LOG2_DEPTH-1:0];
logic [LOG2_DEPTH-1:0] waddr;
logic [LOG2_DEPTH-1:0] raddr;
logic [LOG2_DEPTH-1:0] final_waddr;
logic [LOG2_DEPTH-1:0] final_raddr;
logic [LOG2_DEPTH-1:0] count;
logic empty;
logic internal_empty;

assign access.count = count;
assign access.empty = empty | internal_empty;

always_ff @(negedge clk)
begin
	access.almostfull <= (count > 2**LOG2_DEPTH-20) ? 1'b1 : 1'b0;

	if (access.wfifobram[1] && access.we && !(access.re && empty == 1'b0))
	begin
		empty <= 1'b0;
		count <= count + 1;
	end

	if (access.rfifobram[1] && access.re && empty == 1'b0 && !access.we)
	begin
		count <= count - 1;
		if (count == 1)
		begin
			empty <= 1'b1;
		end
	end

	if (internal_reset)
	begin
		empty <= 1'b1;
		count <= 0;
	end
end

assign final_waddr = (access.wfifobram[0] == 1'b1) ? access.waddr : waddr;
assign final_raddr = (access.rfifobram[0] == 1'b1) ? access.raddr : raddr;

always_ff @(posedge clk)
begin
	internal_empty <= empty;
	access.rvalid <= 1'b0;
	access.rdata <= memory[final_raddr];

	// Write
	if (access.we)
	begin
		memory[final_waddr] <= access.wdata;
		if (access.wfifobram[1] == 1'b1) begin
			waddr <= waddr+1;
		end
	end
	if (access.we)
	begin
		assert ( access.wfifobram[1]^access.wfifobram[0] == 1'b1 )
		else $fatal("fifobram, during write either bram or fifo needs to be selected.");
	end

	// Read
	if (access.re && internal_empty == 1'b0 && access.rfifobram[1] == 1'b1)
	begin
		access.rvalid <= 1'b1;
		raddr <= raddr+1;
	end
	else if (access.re && access.rfifobram[0] == 1'b1)
	begin
		access.rvalid <= 1'b1;
	end
	if (access.re)
	begin
		assert ( access.rfifobram[1]^access.rfifobram[0] == 1'b1 )
		else $fatal("fifobram, during read either bram or fifo needs to be selected.");
	end

	// Reset
	if (internal_reset)
	begin
		waddr <= 0;
		raddr <= 0;
	end
end

endmodule // fifobram