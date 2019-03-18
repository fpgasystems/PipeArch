`include "pipearch_common.vh"

module fifo
#(
	parameter WIDTH = 8,
	parameter LOG2_DEPTH = 5
)
(
	input logic clk,
	input logic reset,
	fifobram_interface.fifo_source access
);

logic internal_reset;
always_ff @(posedge clk)
begin
    internal_reset <= reset;
end

logic [WIDTH-1:0] memory [2**LOG2_DEPTH-1:0];
logic [LOG2_DEPTH-1:0] waddr;
logic [LOG2_DEPTH-1:0] raddr;
logic [LOG2_DEPTH-1:0] count;
logic empty;
logic internal_empty;

assign access.almostfull = (count > 2**LOG2_DEPTH-16) ? 1'b1 : 1'b0;
assign access.count = count;
assign access.empty = empty | internal_empty;

always_ff @(negedge clk)
begin
	if (internal_reset)
	begin
		empty <= 1'b1;
		count <= 0;
	end
	else
	begin
		if (access.we && !(access.re && empty == 1'b0))
		begin
			empty <= 1'b0;
			count <= count + 1;
		end
		
		if (access.re && empty == 1'b0 && !access.we)
		begin
			count <= count - 1;
			if (count == 1)
			begin
				empty <= 1'b1;
			end
		end
	end
end

always_ff @(posedge clk)
begin
	internal_empty <= empty;
	access.rvalid <= 1'b0;

	// Write
	if (access.we)
	begin
		memory[waddr] <= access.wdata;
		waddr <= waddr+1;
	end

	// Read
	if (access.re && internal_empty == 1'b0)
	begin
		access.rvalid <= 1'b1;
		access.rdata <= memory[raddr];
		raddr <= raddr+1;
	end

	// Reset
	if (internal_reset)
	begin
		waddr <= 0;
		raddr <= 0;
	end
end

endmodule // fifo