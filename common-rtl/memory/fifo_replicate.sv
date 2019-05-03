`include "pipearch_common.vh"

module fifo_replicate
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

    function automatic int WriteCheck(logic access_we[NUM_WRITE_CHANNELS], logic access_wfifobram[NUM_WRITE_CHANNELS]);
        int result = 0;
        for (int i = 0; i < NUM_WRITE_CHANNELS; i=i+1) begin
            if (access_we[i] && access_wfifobram[i] == 1'b1)
                result = result + 1;
        end
        return result;
    endfunction

    logic re [NUM_READ_CHANNELS];
    logic [1:0] rfifobram [NUM_READ_CHANNELS];

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) FIFO_region_interface[NUM_READ_CHANNELS]();
    genvar index;
    generate
        for(index = 0; index < NUM_READ_CHANNELS; index = index + 1)
        begin: gen_bram
            fifo
            #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
            FIFO_region (
                .clk, .reset,
                .access(FIFO_region_interface[index].fifo_source)
            );

            always_ff @(posedge clk)
            begin
                re[index] <= read_access[index].re;
                rfifobram[index] <= read_access[index].rfifobram;
            end

            assign FIFO_region_interface[index].re = read_access[index].rfifobram[1] && read_access[index].re;

            assign read_access[index].empty = FIFO_region_interface[index].empty;
            assign read_access[index].rvalid = (FIFO_region_interface[index].rvalid && rfifobram[index] == 2'b10 && re[index]);
            assign read_access[index].rdata = FIFO_region_interface[index].rdata;

            if (NUM_WRITE_CHANNELS == 1) begin
                always_ff @(posedge clk)
                begin
                    // FIFO_region write arbitration
                    FIFO_region_interface[index].we <= 1'b0;
                    if (write_access[0].we && write_access[0].wfifobram[1] == 1'b1) begin
                        FIFO_region_interface[index].we <= 1'b1;
                        FIFO_region_interface[index].wdata <= write_access[0].wdata;
                    end
                end
            end
            else if (NUM_WRITE_CHANNELS == 2) begin
                always_ff @(posedge clk)
                begin
                    // FIFO_region write arbitration
                    FIFO_region_interface[index].we <= 1'b0;
                    if (write_access[0].we && write_access[0].wfifobram[1] == 1'b1) begin
                        FIFO_region_interface[index].we <= 1'b1;
                        FIFO_region_interface[index].wdata <= write_access[0].wdata;
                    end
                    else if (write_access[1].we && write_access[1].wfifobram[1] == 1'b1) begin
                        FIFO_region_interface[index].we <= 1'b1;
                        FIFO_region_interface[index].wdata <= write_access[1].wdata;
                    end

                    if (write_access[0].we || write_access[1].we) begin
                        assert( WriteCheck( {write_access[0].we, write_access[1].we},
                                            {write_access[0].wfifobram[0], write_access[1].wfifobram[0]} ) <= 1)
                        else $fatal("NUM_WRITE_CHANNELS == 2, write_access channels are writing to MEM_region");
                        assert( WriteCheck( {write_access[0].we, write_access[1].we},
                                            {write_access[0].wfifobram[1], write_access[1].wfifobram[1]} ) <= 1)
                        else $fatal("NUM_WRITE_CHANNELS == 2, write_access channels are writing to FIFO_region");
                    end
                end
            end
        end
    endgenerate

    generate
        for (index = 0; index < NUM_WRITE_CHANNELS; index=index+1)
        begin: gen_write
            assign write_access[index].almostfull = FIFO_region_interface[0].almostfull;
            assign write_access[index].count = FIFO_region_interface[0].count;
        end
    endgenerate

endmodule // fifo_replicate