`include "pipearch_common.vh"

module pipearch_scalar2vector
(
    input logic clk,
    input logic reset,

    input logic flush,

    internal_interface.from_commonread getinput,
    internal_interface.to_commonwrite result
);

    logic [479:0] buffer;
    logic [3:0] pointer;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            buffer <= 480'b0;
            pointer <= 0;
        end
        else
        begin
            result.we <= 1'b0;
            if (getinput.rvalid)
            begin
                if (pointer == 15)
                begin
                    result.we <= 1'b1;
                    result.wdata <= {getinput.rdata, buffer};
                    pointer <= 0;
                end
                else
                begin
                    buffer[pointer*32+31 -: 32] <= getinput.rdata;
                    pointer <= pointer + 1;
                end
            end
            if (flush)
            begin
                if (pointer > 0)
                begin
                    result.we <= 1'b1;
                    result.wdata <= {getinput.rdata, buffer};
                end
            end
        end
    end

endmodule // pipearch_scalar2vector