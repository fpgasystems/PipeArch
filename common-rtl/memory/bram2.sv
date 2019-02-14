`include "pipearch_common.vh"

module bram2
#(
	parameter WIDTH = 8,
	parameter LOG2_DEPTH = 5
)
(
	input logic clk,
	fifobram_interface.bram_source access1,
	fifobram_interface.bram_source access2
);

logic [WIDTH-1:0] memory [2**LOG2_DEPTH-1:0];
logic [LOG2_DEPTH-1:0] waddr;
logic [LOG2_DEPTH-1:0] raddr;

always_ff @(posedge clk)
begin
	if (access1.we)
	begin
		memory[access1.waddr] <= access1.wdata;
	end
	if (access2.we)
	begin
		memory[access2.waddr] <= access2.wdata;
	end
	access1.rvalid <= access1.re;
	access1.rdata <= memory[access1.raddr];
	access2.rvalid <= access2.re;
	access2.rdata <= memory[access2.raddr];
end

endmodule // bram