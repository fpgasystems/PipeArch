`include "pipearch_common.vh"

module bram_replicate
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

            if (NUM_WRITE_CHANNELS == 1) begin
                always_ff @(posedge clk)
                begin
                    MEM_region_interface[index].we <= 1'b0;
                    if (write_access[0].we && write_access[0].wfifobram[0]) begin
                        MEM_region_interface[index].we <= 1'b1;
                        MEM_region_interface[index].waddr <= write_access[0].waddr;
                        MEM_region_interface[index].wdata <= write_access[0].wdata;
                    end
                end
            end
            else if (NUM_WRITE_CHANNELS == 2) begin
                always_ff @(posedge clk)
                begin
                    MEM_region_interface[index].we <= 1'b0;
                    if (write_access[0].we && write_access[0].wfifobram[0]) begin
                        MEM_region_interface[index].we <= 1'b1;
                        MEM_region_interface[index].waddr <= write_access[0].waddr;
                        MEM_region_interface[index].wdata <= write_access[0].wdata;
                    end
                    else if (write_access[1].we && write_access[1].wfifobram[0]) begin
                        MEM_region_interface[index].we <= 1'b1;
                        MEM_region_interface[index].waddr <= write_access[1].waddr;
                        MEM_region_interface[index].wdata <= write_access[1].wdata;
                    end

                    if (write_access[0].we || write_access[1].we) begin
                    assert( WriteCheck( {write_access[0].we, write_access[1].we},
                                        {write_access[0].wfifobram[0], write_access[1].wfifobram[0]} ) <= 1)
                    else $fatal("NUM_CHANNELS == 2, access channels are writing to MEM_region");
                end
                end
            end
        end
    endgenerate

    generate
        for (index = 0; index < NUM_WRITE_CHANNELS; index=index+1)
        begin: gen_write
            assign write_access[index].almostfull = 1'b0;
            assign write_access[index].count = 0;
        end
    endgenerate

endmodule // bram_replicate