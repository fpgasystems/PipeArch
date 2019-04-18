`include "pipearch_common.vh"

module region_replicate
#(
    parameter WIDTH = 8,
    parameter LOG2_DEPTH = 5,
    parameter NUM_READ_CHANNELS = 3
)
(
    input logic clk,
    input logic reset,

    fifobram_interface.write_source write_access,
    fifobram_interface.read_source read_access[NUM_READ_CHANNELS]
);

    assign write_access.almostfull = 1'b0;

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) MEM_region_interface[NUM_READ_CHANNELS]();
    genvar index;
    generate
        for(index = 0; index < NUM_READ_CHANNELS; index = index + 1)
        begin: gen_bram
            bram
            #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
            MEM_region (
                .clk,
                .access(MEM_region_interface[index].bram_source)
            );
            assign MEM_region_interface[index].we = write_access.we;
            assign MEM_region_interface[index].waddr = write_access.waddr;
            assign MEM_region_interface[index].wdata = write_access.wdata;

            assign MEM_region_interface[index].re = read_access[index].re;
            assign MEM_region_interface[index].raddr = read_access[index].raddr;
            assign read_access[index].rdata = MEM_region_interface[index].rdata;
            assign read_access[index].rvalid = MEM_region_interface[index].rvalid;
        end
    endgenerate

endmodule // region_replicate