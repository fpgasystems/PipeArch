`include "pipearch_common.vh"

module read_region2fifo
#(
    parameter WIDTH = 512,
    parameter LOG2_DEPTH = 6
)
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,
    input logic [15:0] iterations,

    fifobram_interface.read props_access,
    fifobram_interface.read region_access,
    fifobram_interface.read_source fifo_access
);

    internal_interface #(.WIDTH(WIDTH)) to_FIFO();
    read_region
    REGION_read (
        .clk, .reset,
        .op_start(op_start),
        .configreg(configreg),
        .iterations(iterations),
        .props_access(props_access),
        .region_access(region_access),
        .outfrom_read(to_FIFO.commonread_source)
    );

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) at_FIFO();
    fifo
    #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
    FIFO_input (
        .clk, .reset,
        .access(at_FIFO.fifo_source)
    );
    assign at_FIFO.we = to_FIFO.rvalid;
    assign at_FIFO.wdata = to_FIFO.rdata;
    assign to_FIFO.almostfull = at_FIFO.almostfull;
    assign at_FIFO.re = fifo_access.re;
    assign fifo_access.rvalid = at_FIFO.rvalid;
    assign fifo_access.rdata = at_FIFO.rdata;
    assign fifo_access.empty = at_FIFO.empty;

endmodule // read_region2fifo