`include "pipearch_common.vh"

module region
#(
    parameter WIDTH = 8,
    parameter LOG2_DEPTH = 5,
    parameter NUM_CHANNELS = 3
)
(
    input logic clk,
    input logic reset,

    fifobram_interface.source access[NUM_CHANNELS]
);

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) MEM_region_interface();
    bram
    #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
    MEM_region (
        .clk,
        .access(MEM_region_interface.bram_source)
    );

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) FIFO_region_interface();
    fifo
    #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
    FIFO_region (
        .clk, .reset,
        .access(FIFO_region_interface.fifo_source)
    );

    genvar index;

    // *************************************************************************
    //
    //   Read Channels
    //
    // *************************************************************************
    logic re [NUM_CHANNELS];
    logic [1:0] rfifobram [NUM_CHANNELS];

    generate
        for (index = 0; index < NUM_CHANNELS; index=index+1)
        begin: gen_read
            always_ff @(posedge clk)
            begin
                re[index] <= access[index].re;
                rfifobram[index] <= access[index].rfifobram;
            end

            always_comb
            begin
                // MEM/FIFO_region receive arbitration
                if (MEM_region_interface.rvalid && rfifobram[index] == 2'b01 && re[index]) begin
                    access[index].rvalid = 1'b1;
                    access[index].rdata = MEM_region_interface.rdata;
                end
                else if (FIFO_region_interface.rvalid && rfifobram[index] == 2'b10 && re[index]) begin
                    access[index].rvalid = 1'b1;
                    access[index].rdata = FIFO_region_interface.rdata;
                end
                else begin
                    access[index].rvalid = 1'b0;
                end

                access[index].empty = FIFO_region_interface.empty;
            end
        end
    endgenerate

    generate
        if (NUM_CHANNELS == 1) begin
            always_comb
            begin
                if (access[0].re && access[0].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[0].raddr;
                end
                else begin
                    MEM_region_interface.re = 1'b0;
                    MEM_region_interface.raddr = 0;
                end

                if (access[0].re && access[0].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else begin
                    FIFO_region_interface.re = 1'b0;
                end
            end
        end
        else if (NUM_CHANNELS == 2) begin
            always_comb
            begin
                if (access[0].re && access[0].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[0].raddr;
                end
                else if (access[1].re && access[1].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[1].raddr;
                end
                else begin
                    MEM_region_interface.re = 1'b0;
                    MEM_region_interface.raddr = 0;
                end

                if (access[0].re && access[0].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else if (access[1].re && access[1].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else begin
                    FIFO_region_interface.re = 1'b0;
                end
            end
        end
        else if (NUM_CHANNELS == 3) begin
            always_comb
            begin
                if (access[0].re && access[0].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[0].raddr;
                end
                else if (access[1].re && access[1].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[1].raddr;
                end
                else if (access[2].re && access[2].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[2].raddr;
                end
                else begin
                    MEM_region_interface.re = 1'b0;
                    MEM_region_interface.raddr = 0;
                end

                if (access[0].re && access[0].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else if (access[1].re && access[1].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else if (access[2].re && access[2].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else begin
                    FIFO_region_interface.re = 1'b0;
                end
            end
        end
        else if (NUM_CHANNELS == 4) begin
            always_comb
            begin
                if (access[0].re && access[0].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[0].raddr;
                end
                else if (access[1].re && access[1].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[1].raddr;
                end
                else if (access[2].re && access[2].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[2].raddr;
                end
                else if (access[3].re && access[3].rfifobram == 2'b01) begin
                    MEM_region_interface.re = 1'b1;
                    MEM_region_interface.raddr = access[3].raddr;
                end
                else begin
                    MEM_region_interface.re = 1'b0;
                    MEM_region_interface.raddr = 0;
                end

                if (access[0].re && access[0].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else if (access[1].re && access[1].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else if (access[2].re && access[2].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else if (access[3].re && access[3].rfifobram == 2'b10) begin
                    FIFO_region_interface.re = 1'b1;
                end
                else begin
                    FIFO_region_interface.re = 1'b0;
                end
            end
        end
    endgenerate

    // *************************************************************************
    //
    //   Write Channels
    //
    // *************************************************************************
    generate
        for (index = 0; index < NUM_CHANNELS; index=index+1)
        begin: gen_write
            assign access[index].almostfull = FIFO_region_interface.almostfull;
            assign access[index].count = FIFO_region_interface.count;
        end

        if (NUM_CHANNELS == 1) begin
            always_ff @(posedge clk)
            begin
                // MEM_region write arbitration
                MEM_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[0].waddr;
                    MEM_region_interface.wdata <= access[0].wdata;
                end

                // FIFO_region write arbitration
                FIFO_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[0].wdata;
                end
            end
        end
        else if (NUM_CHANNELS == 2) begin
            always_ff @(posedge clk)
            begin
                // MEM_region write arbitration
                MEM_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[0].waddr;
                    MEM_region_interface.wdata <= access[0].wdata;
                end
                else if (access[1].we && access[1].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[1].waddr;
                    MEM_region_interface.wdata <= access[1].wdata;
                end

                // FIFO_region write arbitration
                FIFO_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[0].wdata;
                end
                else if (access[1].we && access[1].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[1].wdata;
                end
            end
        end
        else if (NUM_CHANNELS == 3) begin
            always_ff @(posedge clk)
            begin
                // MEM_region write arbitration
                MEM_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[0].waddr;
                    MEM_region_interface.wdata <= access[0].wdata;
                end
                else if (access[1].we && access[1].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[1].waddr;
                    MEM_region_interface.wdata <= access[1].wdata;
                end
                else if (access[2].we && access[2].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[2].waddr;
                    MEM_region_interface.wdata <= access[2].wdata;
                end

                // FIFO_region write arbitration
                FIFO_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[0].wdata;
                end
                else if (access[1].we && access[1].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[1].wdata;
                end
                else if (access[2].we && access[2].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[2].wdata;
                end
            end
        end
        else if (NUM_CHANNELS == 4) begin
            always_ff @(posedge clk)
            begin
                // MEM_region write arbitration
                MEM_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[0].waddr;
                    MEM_region_interface.wdata <= access[0].wdata;
                end
                else if (access[1].we && access[1].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[1].waddr;
                    MEM_region_interface.wdata <= access[1].wdata;
                end
                else if (access[2].we && access[2].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[2].waddr;
                    MEM_region_interface.wdata <= access[2].wdata;
                end
                else if (access[3].we && access[3].wfifobram[0] == 1'b1) begin
                    MEM_region_interface.we <= 1'b1;
                    MEM_region_interface.waddr <= access[3].waddr;
                    MEM_region_interface.wdata <= access[3].wdata;
                end

                // FIFO_region write arbitration
                FIFO_region_interface.we <= 1'b0;
                if (access[0].we && access[0].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[0].wdata;
                end
                else if (access[1].we && access[1].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[1].wdata;
                end
                else if (access[2].we && access[2].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[2].wdata;
                end
                else if (access[3].we && access[3].wfifobram[1] == 1'b1) begin
                    FIFO_region_interface.we <= 1'b1;
                    FIFO_region_interface.wdata <= access[3].wdata;
                end
            end
        end
    endgenerate

endmodule // region