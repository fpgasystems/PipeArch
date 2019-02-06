`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module pipearch_load
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [5],
    input t_ccip_clAddr in_addr,

    // CCI-P request/response
    input  c0TxAlmFull,
    input  t_if_ccip_c0_Rx cp2af_sRx_c0,
    output t_if_ccip_c0_Tx af2cp_sTx_c0,

    internal_interface.to_commonwrite into_write
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;
    t_readstate receive_state;

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) prefetch_fifo_access();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .reset,
        .access(prefetch_fifo_access.fifo_source)
    );
    assign prefetch_fifo_access.we = cci_c0Rx_isReadRsp(cp2af_sRx_c0) && (receive_state == STATE_READ);
    assign prefetch_fifo_access.wdata = cp2af_sRx_c0.data;
    always_ff @(posedge clk)
    begin
        prefetch_fifo_access.re <= !(prefetch_fifo_access.empty) && !(into_write.almostfull);
    end

    t_ccip_clAddr DRAM_load_offset;
    t_ccip_clAddr running_DRAM_load_offset;
    logic [31:0] DRAM_load_length;
    logic enable_multiline;

    // Counters
    logic [31:0] num_requested_lines;
    logic [31:0] num_received_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    always_ff @(posedge clk)
    begin
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_access.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);

        af2cp_sTx_c0.hdr <= t_cci_c0_ReqMemHdr'(0);
        af2cp_sTx_c0.valid <= 1'b0;

        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;
            num_requested_lines <= 32'b0;
            num_received_lines <= 32'b0;
            into_write.we <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin
            
            // =================================
            //
            //   Request State Machine
            //
            // =================================
            case (request_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        DRAM_load_offset <= in_addr + regs[3];
                        running_DRAM_load_offset <= in_addr + regs[3];
                        DRAM_load_length <= 32'(regs[4][30:0]);
                        enable_multiline <= regs[4][31];
                        num_requested_lines <= 32'b0;
                        if (regs[4] == 0)
                        begin
                            request_state <= STATE_DONE;
                        end
                        else
                        begin
                            request_state <= STATE_READ;
                        end
                    end
                end

                STATE_READ:
                begin
                    if (!c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                    begin
                        if (enable_multiline && 
                            (num_requested_lines+4 < DRAM_load_length) && 
                            (running_DRAM_load_offset[1:0] == 2'b0))
                        begin
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'(running_DRAM_load_offset);
                            af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                            af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_4;
                            num_requested_lines <= num_requested_lines + 4;
                            running_DRAM_load_offset <= running_DRAM_load_offset + 4;
                        end
                        else if (enable_multiline && 
                            (num_requested_lines+2 < DRAM_load_length) &&
                            (running_DRAM_load_offset[0] == 1'b0))
                        begin
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'(running_DRAM_load_offset);
                            af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                            af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_2;
                            num_requested_lines <= num_requested_lines + 2;
                            running_DRAM_load_offset <= running_DRAM_load_offset + 2;
                        end
                        else if (num_requested_lines < DRAM_load_length)
                        begin
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'(running_DRAM_load_offset);
                            af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                            af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_1;
                            num_requested_lines <= num_requested_lines + 1;
                            running_DRAM_load_offset <= running_DRAM_load_offset + 1;
                        end
                    end

                    if (num_requested_lines == DRAM_load_length)
                    begin
                        request_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    request_state <= STATE_IDLE;
                end
            endcase


            into_write.we <= 1'b0;
            op_done <= 1'b0;
            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            case (receive_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        num_received_lines <= 32'b0;
                        if (regs[4] == 0)
                        begin
                            receive_state <= STATE_DONE;
                        end
                        else
                        begin
                            receive_state <= STATE_READ;
                        end
                    end
                end

                STATE_READ:
                begin
                    if (prefetch_fifo_access.rvalid)
                    begin
                        into_write.we <= 1'b1;
                        into_write.wdata <= prefetch_fifo_access.rdata;
                        num_received_lines <= num_received_lines + 1;
                        if (num_received_lines == DRAM_load_length-1)
                        begin
                            receive_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    receive_state <= STATE_IDLE;
                end
            endcase
        end
    end

endmodule // pipearch_load