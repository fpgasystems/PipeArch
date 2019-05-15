`include "pipearch_common.vh"

module region_exclusive
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

    fifobram_interface #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH)) FIFOBRAM_region_interface();
    fifobram
    #(.WIDTH(WIDTH), .LOG2_DEPTH(LOG2_DEPTH))
    FIFOBRAM_region (
        .clk, .reset,
        .access(FIFOBRAM_region_interface)
    );

    genvar index;

// synthesis translate_off
    function automatic int ReadCheck(logic access_re[NUM_READ_CHANNELS]);
        int result = 0;
        for (int i = 0; i < NUM_READ_CHANNELS; i=i+1) begin
            if (access_re[i])
                result = result + 1;
        end
        return result;
    endfunction

    function automatic int WriteCheck(logic access_we[NUM_WRITE_CHANNELS]);
        int result = 0;
        for (int i = 0; i < NUM_WRITE_CHANNELS; i=i+1) begin
            if (access_we[i])
                result = result + 1;
        end
        return result;
    endfunction
// synthesis translate_on

    // *************************************************************************
    //
    //   Read Channels
    //
    // *************************************************************************
    logic re [NUM_READ_CHANNELS];
    logic [1:0] rfifobram [NUM_READ_CHANNELS];

    generate
        for (index = 0; index < NUM_READ_CHANNELS; index=index+1)
        begin: gen_read
            always_ff @(posedge clk)
            begin
                re[index] <= read_access[index].re;
                rfifobram[index] <= read_access[index].rfifobram;
            end
            assign read_access[index].empty = FIFOBRAM_region_interface.empty;
            assign read_access[index].rvalid = FIFOBRAM_region_interface.rvalid && re[index];
            assign read_access[index].rdata = FIFOBRAM_region_interface.rdata;

        end
    endgenerate

    generate
        if (NUM_READ_CHANNELS == 1) begin
            always_comb
            begin
                FIFOBRAM_region_interface.re = read_access[0].re;
                FIFOBRAM_region_interface.raddr = read_access[0].raddr;
                FIFOBRAM_region_interface.rfifobram = read_access[0].rfifobram;
            end
        end
        else if (NUM_READ_CHANNELS == 2) begin
            always_comb
            begin
                if (read_access[0].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[0].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[0].rfifobram;
                end
                else if (read_access[1].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[1].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[1].rfifobram;
                end
                else begin
                    FIFOBRAM_region_interface.re = 1'b0;
                    FIFOBRAM_region_interface.raddr = 0;
                    FIFOBRAM_region_interface.rfifobram = 0;
                end
// synthesis translate_off
                if (read_access[0].re || read_access[1].re) begin
                    assert ( ReadCheck({read_access[0].re, read_access[1].re}) <= 1 )
                    else $fatal("NUM_READ_CHANNELS == 2, read_access channels are reading from FIFOBRAM_region");
                end
// synthesis translate_on
            end
        end
        else if (NUM_READ_CHANNELS == 3) begin
            always_comb
            begin
                if (read_access[0].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[0].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[0].rfifobram;
                end
                else if (read_access[1].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[1].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[1].rfifobram;
                end
                else if (read_access[2].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[2].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[2].rfifobram;
                end
                else begin
                    FIFOBRAM_region_interface.re = 1'b0;
                    FIFOBRAM_region_interface.raddr = 0;
                    FIFOBRAM_region_interface.rfifobram = 0;
                end
// synthesis translate_off
                if (read_access[0].re || read_access[1].re || read_access[2].re) begin
                    assert ( ReadCheck({read_access[0].re, read_access[1].re, read_access[2].re}) <= 1 )
                    else $fatal("NUM_READ_CHANNELS == 3, read_access channels are reading from FIFOBRAM_region");
                end
// synthesis translate_on
            end
        end
        else if (NUM_READ_CHANNELS == 4) begin
            always_comb
            begin
                if (read_access[0].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[0].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[0].rfifobram;
                end
                else if (read_access[1].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[1].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[1].rfifobram;
                end
                else if (read_access[2].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[2].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[2].rfifobram;
                end
                else if (read_access[3].re) begin
                    FIFOBRAM_region_interface.re = 1'b1;
                    FIFOBRAM_region_interface.raddr = read_access[3].raddr;
                    FIFOBRAM_region_interface.rfifobram = read_access[3].rfifobram;
                end
                else begin
                    FIFOBRAM_region_interface.re = 1'b0;
                    FIFOBRAM_region_interface.raddr = 0;
                    FIFOBRAM_region_interface.rfifobram = 0;
                end
// synthesis translate_off
                if (read_access[0].re || read_access[1].re || read_access[2].re || read_access[3].re) begin
                    assert ( ReadCheck({read_access[0].re, read_access[1].re, read_access[2].re, read_access[3].re}) <= 1 )
                    else $fatal("NUM_READ_CHANNELS == 4, read_access channels are reading from FIFOBRAM_region");
                end
// synthesis translate_on
            end
        end
    endgenerate

    // *************************************************************************
    //
    //   Write Channels
    //
    // *************************************************************************
    generate
        for (index = 0; index < NUM_WRITE_CHANNELS; index=index+1)
        begin: gen_write
            assign write_access[index].almostfull = FIFOBRAM_region_interface.almostfull;
            assign write_access[index].count = FIFOBRAM_region_interface.count;
        end

        if (NUM_WRITE_CHANNELS == 1) begin
            always_ff @(posedge clk)
            begin
                // FIFOBRAM_region write arbitration
                FIFOBRAM_region_interface.we <= 1'b0;
                if (write_access[0].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[0].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[0].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[0].wfifobram;
                end
            end
        end
        else if (NUM_WRITE_CHANNELS == 2) begin
            always_ff @(posedge clk)
            begin
                // FIFOBRAM_region write arbitration
                FIFOBRAM_region_interface.we <= 1'b0;
                if (write_access[0].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[0].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[0].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[0].wfifobram;
                end
                else if (write_access[1].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[1].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[1].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[1].wfifobram;
                end
// synthesis translate_off
                if (write_access[0].we || write_access[1].we) begin
                    assert( WriteCheck( {write_access[0].we, write_access[1].we} ) <= 1)
                    else $fatal("NUM_WRITE_CHANNELS == 2, write_access channels are writing to MEM_region");
                end
// synthesis translate_on
            end
        end
        else if (NUM_WRITE_CHANNELS == 3) begin
            always_ff @(posedge clk)
            begin
                // FIFOBRAM_region write arbitration
                FIFOBRAM_region_interface.we <= 1'b0;
                if (write_access[0].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[0].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[0].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[0].wfifobram;
                end
                else if (write_access[1].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[1].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[1].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[1].wfifobram;
                end
                else if (write_access[2].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[2].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[2].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[2].wfifobram;
                end
// synthesis translate_off
                if (write_access[0].we || write_access[1].we || write_access[2].we) begin
                    assert( WriteCheck( {write_access[0].we, write_access[1].we, write_access[2].we} ) <= 1)
                    else $fatal("NUM_WRITE_CHANNELS == 3, write_access channels are writing to MEM_region");
                end
// synthesis translate_on
            end
        end
        else if (NUM_WRITE_CHANNELS == 4) begin
            always_ff @(posedge clk)
            begin
                // FIFOBRAM_region write arbitration
                FIFOBRAM_region_interface.we <= 1'b0;
                if (write_access[0].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[0].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[0].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[0].wfifobram;
                end
                else if (write_access[1].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[1].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[1].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[1].wfifobram;
                end
                else if (write_access[2].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[2].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[2].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[2].wfifobram;
                end
                else if (write_access[3].we) begin
                    FIFOBRAM_region_interface.we <= 1'b1;
                    FIFOBRAM_region_interface.waddr <= write_access[3].waddr;
                    FIFOBRAM_region_interface.wdata <= write_access[3].wdata;
                    FIFOBRAM_region_interface.wfifobram <= write_access[3].wfifobram;
                end
// synthesis translate_off
                if (write_access[0].we || write_access[1].we || write_access[2].we || write_access[3].we) begin
                    assert( WriteCheck( {write_access[0].we, write_access[1].we, write_access[2].we, write_access[3].we} ) <= 1)
                    else $fatal("NUM_WRITE_CHANNELS == 4, write_access channels are writing to MEM_region");
                end
// synthesis translate_on
            end
        end
    endgenerate

endmodule // region_exclusive