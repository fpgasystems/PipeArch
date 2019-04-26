`include "pipearch_common.vh"

module read_region
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,
    input logic [15:0] iterations,

    fifobram_interface.read props_access,
    fifobram_interface.read region_access,
    internal_interface.commonread_source outfrom_read
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_BRAM_READ,
        STATE_FIFO_READ,
        STATE_FETCH_PROPS,
        STATE_RECEIVE_PROPS,
        STATE_READ_WITH_PROPS
    } t_readstate;
    t_readstate read_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic[15:0] num_iterations;
    access_properties memory_read;
    logic [CLDATA_WIDTH-1:0] accessprops_data;
    logic [LOG2_ACCESS_SIZE-1:0] current_offset;
    logic [LOG2_ACCESS_SIZE-1:0] current_length;
    logic [17:0] accessprops_raddr;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [LOG2_ACCESS_SIZE-1:0] num_requested_lines;
    logic [15:0] num_performed_iterations;
    logic [3:0] accessprops_position;

    assign accessprops_position = accessprops_raddr[3:0];
    assign current_offset = accessprops_data[accessprops_position*32+13 -: 14];
    assign current_length = accessprops_data[accessprops_position*32+29 -: 14];

    always_ff @(posedge clk)
    begin
        props_access.re <= 1'b0;
        region_access.re <= 1'b0;
        region_access.rfifobram <= 2'b00;

        outfrom_read.rvalid <= region_access.rvalid;
        outfrom_read.rdata <= region_access.rdata;

        case (read_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    memory_read.write_bram <= configreg[30];
                    memory_read.write_fifo <= configreg[31];
                    memory_read.offset <= configreg[13:0];
                    memory_read.length <= configreg[29:16];
                    num_iterations <= iterations;
                    memory_read.use_local_props <= configreg[14];
                    memory_read.keep_count_along_iterations <= configreg[15];
                    accessprops_raddr <= {configreg[13:0], 4'b0000};
                    // *************************************************************************
                    num_requested_lines <= 0;
                    num_performed_iterations <= 0;
                    if (configreg[14])
                    begin
                        read_state <= STATE_FETCH_PROPS;
                    end
                    else if (configreg[29:16] == 16'b0 || configreg[31:30] == 2'b0)
                    begin
                        read_state <= STATE_IDLE;
                    end
                    else if (configreg[31])
                    begin
                        read_state <= STATE_FIFO_READ;
                    end
                    else
                    begin
                        read_state <= STATE_BRAM_READ;
                    end
                end
            end

            STATE_FETCH_PROPS:
            begin
                props_access.re <= 1'b1;
                props_access.rfifobram <= 2'b01;
                props_access.raddr <= accessprops_raddr >> 4;
                read_state <= STATE_RECEIVE_PROPS;
            end

            STATE_RECEIVE_PROPS:
            begin
                if (props_access.rvalid)
                begin
                    accessprops_data <= props_access.rdata;
                    read_state <= STATE_READ_WITH_PROPS;
                end
            end

            STATE_READ_WITH_PROPS:
            begin
                if (num_requested_lines < current_length && !outfrom_read.almostfull)
                begin
                    region_access.re <= 1'b1;
                    region_access.rfifobram <= 2'b01;
                    region_access.raddr <= current_offset + num_requested_lines;
                    num_requested_lines <= num_requested_lines + 1;

                    if (num_requested_lines == current_length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_requested_lines <= 0;
                            if (memory_read.keep_count_along_iterations)
                            begin
                                accessprops_raddr <= accessprops_raddr + memory_read.length;
                            end
                            if (num_performed_iterations[3:0] == 4'd15)
                            begin
                                read_state <= STATE_FETCH_PROPS;
                            end
                        end
                    end
                end
            end

            STATE_BRAM_READ:
            begin
                if (num_requested_lines < memory_read.length && !outfrom_read.almostfull)
                begin
                    region_access.re <= 1'b1;
                    region_access.rfifobram <= 2'b01;
                    region_access.raddr <= memory_read.offset + num_requested_lines;
                    num_requested_lines <= num_requested_lines + 1;

                    if (num_requested_lines == memory_read.length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_requested_lines <= 0;
                            if (memory_read.keep_count_along_iterations)
                            begin
                                memory_read.offset <= memory_read.offset + memory_read.length;
                            end
                        end
                    end
                end
            end

            STATE_FIFO_READ:
            begin
                if (num_requested_lines < memory_read.length && !region_access.empty && !outfrom_read.almostfull)
                begin
                    region_access.re <= 1'b1;
                    region_access.rfifobram <= 2'b10;
                    num_requested_lines <= num_requested_lines + 1;

                    if (num_requested_lines == memory_read.length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_requested_lines <= 0;
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            read_state <= STATE_IDLE;
        end
    end

endmodule // read_bram