`include "pipearch_common.vh"

module bram_replicate
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

    logic re [NUM_READ_CHANNELS];
    logic [1:0] rfifobram [NUM_READ_CHANNELS];

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

            always_ff @(posedge clk)
            begin
                re[index] <= read_access[index].re;
                rfifobram[index] <= read_access[index].rfifobram;
            end

            assign MEM_region_interface[index].we = write_access.wfifobram[0] ? write_access.we : 1'b0;
            assign MEM_region_interface[index].waddr = write_access.waddr;
            assign MEM_region_interface[index].wdata = write_access.wdata;

            assign MEM_region_interface[index].re = read_access[index].rfifobram[0] ? read_access[index].re : 1'b0;
            assign MEM_region_interface[index].raddr = read_access[index].raddr;

            always_comb
            begin
                // MEM_region receive arbitration
                if (MEM_region_interface[index].rvalid && rfifobram[index] == 2'b01 && re[index]) begin
                    read_access[index].rvalid = 1'b1;
                    read_access[index].rdata = MEM_region_interface[index].rdata;
                end
                else begin
                    read_access[index].rvalid = 1'b0;
                end
            end
        end
    endgenerate

    assign write_access.almostfull = 1'b0;

endmodule // bram_replicate