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

logic [WIDTH-1:0] memory [2**LOG2_DEPTH-1:0];
logic [LOG2_DEPTH-1:0] waddr;
logic [LOG2_DEPTH-1:0] raddr;
logic [LOG2_DEPTH-1:0] count;
logic empty;

assign access.almostfull = (count > 2**LOG2_DEPTH-16) ? 1'b1 : 1'b0;
assign access.count = count;
assign access.empty = empty;

always_ff @(posedge clk)
begin
	if (reset)
	begin
		waddr <= 0;
		raddr <= 0;
		count <= 0;
		empty <= 1'b1;
	end
	else
	begin

		// Write
		if (access.we)
		begin
			memory[waddr] <= access.wdata;
			waddr <= waddr+1;

			if (!(access.re && empty == 1'b0))
			begin
				count <= count + 1;
				empty <= 1'b0;
			end
		end

		// Read
		access.rvalid <= 1'b0;
		if (access.re && empty == 1'b0)
		begin
			access.rvalid <= 1'b1;
			access.rdata <= memory[raddr];
			raddr <= raddr+1;

			if (!access.we)
			begin
				count <= count - 1;
				if (count == 1)
				begin
					empty <= 1'b1;
				end
			end
		end
	end
end

endmodule // fifo