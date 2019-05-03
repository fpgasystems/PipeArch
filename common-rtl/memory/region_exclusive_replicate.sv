`include "pipearch_common.vh"

module region_exclusive_replicate
#(
    parameter WIDTH = 8,
    parameter LOG2_DEPTH = 5,
    parameter NUM_WRITE_CHANNELS = 1,
    parameter NUM_READ_CHANNELS = 3
)
(
    input logic clk,
    input logic reset,

    fifobram_interface.write_source write_access[NUM_WRITE_CHANNELS],
    fifobram_interface.read_source read_access[NUM_READ_CHANNELS]
);

    function automatic int WriteCheck(logic access_we[NUM_WRITE_CHANNELS]);
        int result = 0;
        for (int i = 0; i < NUM_WRITE_CHANNELS; i=i+1) begin
            if (access_we[i])
                result = result + 1;
        end
        return result;
    endfunction

    logic re [NUM_READ_CHANNELS];
    logic [1:0] rfifobram [NUM_READ_CHANNELS];

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) FIFOBRAM_region_interface[NUM_READ_CHANNELS]();
    genvar index;
    generate
        for(index = 0; index < NUM_READ_CHANNELS; index = index + 1)
        begin: gen_fifobram

            fifobram
            #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
            FIFOBRAM_region (
                .clk, .reset,
                .access(FIFOBRAM_region_interface[index])
            );

            always_ff @(posedge clk)
            begin
                re[index] <= read_access[index].re;
                rfifobram[index] <= read_access[index].rfifobram;
            end

            assign FIFOBRAM_region_interface[index].re = read_access[index].re;
            assign FIFOBRAM_region_interface[index].raddr = read_access[index].raddr;
            assign FIFOBRAM_region_interface[index].rfifobram = read_access[index].rfifobram;

            assign read_access[index].empty = FIFOBRAM_region_interface[index].empty;
            assign read_access[index].rvalid = FIFOBRAM_region_interface[index].rvalid && re[index];
            assign read_access[index].rdata = FIFOBRAM_region_interface[index].rdata;

            if (NUM_WRITE_CHANNELS == 1) begin
                always_ff @(posedge clk)
                begin
                    // FIFOBRAM_region write arbitration
                    FIFOBRAM_region_interface[index].we <= 1'b0;
                    if (write_access[0].we) begin
                        FIFOBRAM_region_interface[index].we <= 1'b1;
                        FIFOBRAM_region_interface[index].waddr <= write_access[0].waddr;
                        FIFOBRAM_region_interface[index].wdata <= write_access[0].wdata;
                        FIFOBRAM_region_interface[index].wfifobram <= write_access[0].wfifobram;
                    end
                end
            end
            else if (NUM_WRITE_CHANNELS == 2) begin
                always_ff @(posedge clk)
                begin
                    // FIFOBRAM_region write arbitration
                    FIFOBRAM_region_interface[index].we <= 1'b0;
                    if (write_access[0].we) begin
                        FIFOBRAM_region_interface[index].we <= 1'b1;
                        FIFOBRAM_region_interface[index].waddr <= write_access[0].waddr;
                        FIFOBRAM_region_interface[index].wdata <= write_access[0].wdata;
                        FIFOBRAM_region_interface[index].wfifobram <= write_access[0].wfifobram;
                    end
                    else if (write_access[1].we) begin
                        FIFOBRAM_region_interface[index].we <= 1'b1;
                        FIFOBRAM_region_interface[index].waddr <= write_access[1].waddr;
                        FIFOBRAM_region_interface[index].wdata <= write_access[1].wdata;
                        FIFOBRAM_region_interface[index].wfifobram <= write_access[1].wfifobram;
                    end

                    if (write_access[0].we || write_access[1].we) begin
                        assert( WriteCheck( {write_access[0].we, write_access[1].we} ) <= 1)
                        else $fatal("NUM_WRITE_CHANNELS == 2, write_access channels are writing to MEM_region");
                    end
                end
            end
        end
    endgenerate

    generate
        for (index = 0; index < NUM_WRITE_CHANNELS; index=index+1)
        begin: gen_write
            assign write_access[index].almostfull = FIFOBRAM_region_interface[0].almostfull;
            assign write_access[index].count = FIFOBRAM_region_interface[0].count;
        end
    endgenerate

endmodule // region_exclusive_replicate