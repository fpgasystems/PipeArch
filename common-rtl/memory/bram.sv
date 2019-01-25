`include "pipearch_common.vh"

module bram
#(
	parameter WIDTH = 8,
	parameter LOG2_DEPTH = 5
)
(
    input logic clk,
    fifobram_interface.bram_source access
);

logic [WIDTH-1:0] memory [2**LOG2_DEPTH-1:0];
logic [LOG2_DEPTH-1:0] waddr;
logic [LOG2_DEPTH-1:0] raddr;

always_ff @(posedge clk)
begin
	if (access.we)
	begin
		memory[access.waddr] <= access.wdata;
	end
	access.rvalid <= access.re;
	access.rdata <= memory[access.raddr];
end

endmodule // bram