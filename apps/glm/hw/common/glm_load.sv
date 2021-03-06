`include "pipearch_common.vh"
`include "glm_common.vh"

module glm_load
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic in_trigger_dma,
    input logic [31:0] regs [7],
    input t_claddr in_addr,

    // request/response
    dma_read_interface DMA_read,

    fifobram_interface.write REGION0_write,
    fifobram_interface.write REGION1_write,
    fifobram_interface.write REGION2_write,
    fifobram_interface.write REGION_prefetch_write,
    fifobram_interface.write MEM_localprops_write,
    fifobram_interface.write MEM_accessprops_write,
    fifobram_interface.read MEM_accessprops_read
);
    logic internal_reset;
    always_ff @(posedge clk)
    begin
        internal_reset <= reset;
    end

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [3:0]
    {
        STATE_IDLE,
        STATE_PREPROCESS,
        STATE_FETCH_ACCESSPROPS,
        STATE_RECEIVE_ACCESSPROPS,
        STATE_ADD_ACCESSPROPS,
        STATE_DMA_TRIGGER,
        STATE_WRITE_TRIGGER,
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic trigger_dma;
    logic[31:0] offset_by_index[3];
    logic [$bits(t_claddr)-32-1:0] upper_part_address;
    logic [31:0] running_DRAM_load_offset;
    logic [31:0] DRAM_load_length;
    logic enable_multiline;
    logic use_accessprops;
    logic [3+LOG2_MEMORY_SIZE-1:0] accessprops_raddr;
    logic [31:0] REGION0_accessproperties;
    logic [31:0] REGION1_accessproperties;
    logic [31:0] REGION2_accessproperties;
    logic [31:0] MEM_accessprops_accessproperties;
    logic [31:0] MEM_localprops_accessproperties;
    logic [31:0] REGION_prefetch_accessproperties;

    // *************************************************************************
    //
    //   Load Channels
    //
    // *************************************************************************
    logic write_trigger;
    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_load();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_props_access[6]();

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_load_to_REGION0();
    write_region
    write_REGION0 (
        .clk, .reset(internal_reset),
        .op_start(write_trigger),
        .configreg(REGION0_accessproperties),
        .iterations(16'd1),
        .into_write(from_load_to_REGION0.commonwrite_source),
        .props_access(dummy_props_access[0].read),
        .region_access(REGION0_write)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_load_to_REGION1();
    write_region
    write_REGION1 (
        .clk, .reset(internal_reset),
        .op_start(write_trigger),
        .configreg(REGION1_accessproperties),
        .iterations(16'd1),
        .into_write(from_load_to_REGION1.commonwrite_source),
        .props_access(dummy_props_access[1].read),
        .region_access(REGION1_write)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_load_to_REGION2();
    write_region
    write_REGION2 (
        .clk, .reset(internal_reset),
        .op_start(write_trigger),
        .configreg(REGION2_accessproperties),
        .iterations(16'd1),
        .into_write(from_load_to_REGION2.commonwrite_source),
        .props_access(dummy_props_access[2].read),
        .region_access(REGION2_write)
    );

    internal_interface #(.WIDTH(512)) from_load_to_MEM_accessprops();
    write_region
    write_MEM_accessprops_inst (
        .clk, .reset(internal_reset),
        .op_start(write_trigger),
        .configreg(MEM_accessprops_accessproperties),
        .iterations(16'd1),
        .into_write(from_load_to_MEM_accessprops.commonwrite_source),
        .props_access(dummy_props_access[3].read),
        .region_access(MEM_accessprops_write)
    );

    internal_interface #(.WIDTH(512)) from_load_to_MEM_localprops();
    write_region
    write_MEM_localprops_inst (
        .clk, .reset(internal_reset),
        .op_start(write_trigger),
        .configreg(MEM_localprops_accessproperties),
        .iterations(16'd1),
        .into_write(from_load_to_MEM_localprops.commonwrite_source),
        .props_access(dummy_props_access[4].read),
        .region_access(MEM_localprops_write)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_load_to_REGION_prefetch();
    write_region
    write_REGION_prefetch (
        .clk, .reset(internal_reset),
        .op_start(write_trigger),
        .configreg(REGION_prefetch_accessproperties),
        .iterations(16'd1),
        .into_write(from_load_to_REGION_prefetch.commonwrite_source),
        .props_access(dummy_props_access[5].read),
        .region_access(REGION_prefetch_write)
    );

    always_ff @(posedge clk)
    begin
        from_load_to_REGION0.we <= from_load.we;
        from_load_to_REGION0.wdata <= from_load.wdata;
        from_load_to_REGION1.we <= from_load.we;
        from_load_to_REGION1.wdata <= from_load.wdata;
        from_load_to_REGION2.we <= from_load.we;
        from_load_to_REGION2.wdata <= from_load.wdata;
        from_load_to_MEM_accessprops.we <= from_load.we;
        from_load_to_MEM_accessprops.wdata <= from_load.wdata;
        from_load_to_MEM_localprops.we <= from_load.we;
        from_load_to_MEM_localprops.wdata <= from_load.wdata;
        from_load_to_REGION_prefetch.we <= from_load.we;
        from_load_to_REGION_prefetch.wdata <= from_load.wdata;
        from_load.almostfull <= from_load_to_REGION0.almostfull |
                                from_load_to_REGION1.almostfull |
                                from_load_to_REGION2.almostfull |
                                from_load_to_MEM_accessprops.almostfull |
                                from_load_to_MEM_localprops.almostfull |
                                from_load_to_REGION_prefetch.almostfull;
    end 

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [2:0] accessprops_position;
    logic [31:0] accessprops_DRAM_offset;
    logic [31:0] accessprops_DRAM_length;
    logic [1:0] offset_accumulate;
    logic [31:0] num_requested_lines;
    logic [31:0] num_requested_lines_plus2;
    logic [31:0] num_requested_lines_plus4;
    logic [31:0] num_received_lines;
    logic [31:0] num_forward_request_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    // *************************************************************************
    //
    //   Receive FIFO
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) prefetch_fifo_access();
    fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .reset(internal_reset),
        .access(prefetch_fifo_access.fifo_source)
    );
    
    always_ff @(posedge clk)
    begin
        prefetch_fifo_access.we <= 1'b0;
        if (op_start)
        begin
            num_received_lines <= 32'b0;
        end
        if (DMA_read.rx_read.rvalid && request_state == STATE_READ)
        begin
            num_received_lines <= num_received_lines + 1;
            prefetch_fifo_access.we <= 1'b1;
            prefetch_fifo_access.wdata <= DMA_read.rx_read.rdata;
        end
        prefetch_fifo_access.re <= !(prefetch_fifo_access.empty) && !(from_load.almostfull);
    end

    always_ff @(posedge clk)
    begin
        // =================================
        //
        //   Calculate Allowed Lines
        //
        // =================================
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_access.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);

        DMA_read.control.start <= 1'b0;
        DMA_read.tx_read.re <= 1'b0;

        from_load.we <= 1'b0;
        MEM_accessprops_read.re <= 1'b0;
        MEM_accessprops_read.rfifobram <= 2'b01;
        op_done <= 1'b0;

        // =================================
        //
        //   Request State Machine
        //
        // =================================
        write_trigger <= 1'b0;

        case (request_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    trigger_dma <= in_trigger_dma;
                    offset_by_index[0] <= regs[0];
                    offset_by_index[1] <= regs[1];
                    offset_by_index[2] <= regs[2];
                    upper_part_address <= in_addr[$bits(t_claddr)-1:32];
                    running_DRAM_load_offset <= (regs[3][31] == 1'b1) ? in_addr[31:0] : in_addr[31:0] + regs[3][29:0];
                    DRAM_load_length <= regs[4][30:0];
                    enable_multiline <= regs[4][31];
                    use_accessprops <= regs[3][31];
                    accessprops_raddr <= {regs[3][LOG2_MEMORY_SIZE-1:0], 3'b000};
                    REGION0_accessproperties <= regs[6][0] ? regs[5] : 0;
                    REGION1_accessproperties <= regs[6][1] ? regs[5] : 0;
                    REGION2_accessproperties <= regs[6][2] ? regs[5] : 0;
                    MEM_accessprops_accessproperties <= regs[6][3] ? regs[5] : 0;
                    MEM_localprops_accessproperties <= regs[6][4] ? regs[5] : 0;
                    REGION_prefetch_accessproperties <= regs[6][5] ? regs[5] : 0;
                    // *************************************************************************
                    accessprops_position <= 0;
                    offset_accumulate <= 2'b0;
                    num_requested_lines <= 32'b0;
                    num_requested_lines_plus4 <= 32'd4;
                    num_requested_lines_plus2 <= 32'd2;
                    num_forward_request_lines <= 32'b0;
                    if (regs[4][30:0] == 0 && regs[3][31] == 0)
                    begin
                        request_state <= STATE_DONE;
                    end
                    else
                    begin
                        request_state <= STATE_PREPROCESS;
                    end
                end
            end

            STATE_PREPROCESS:
            begin
                if (use_accessprops)
                begin
                    accessprops_raddr <= accessprops_raddr + offset_by_index[offset_accumulate];
                    if (offset_accumulate == 2)
                    begin
                        request_state <= STATE_FETCH_ACCESSPROPS;
                    end
                end
                else
                begin
                    running_DRAM_load_offset <= running_DRAM_load_offset + offset_by_index[offset_accumulate];
                    if (offset_accumulate == 2)
                    begin
                        request_state <= trigger_dma ? STATE_DMA_TRIGGER : STATE_WRITE_TRIGGER;
                    end
                end
                offset_accumulate <= offset_accumulate + 1;
            end

            STATE_FETCH_ACCESSPROPS:
            begin
                MEM_accessprops_read.re <= 1'b1;
                MEM_accessprops_read.raddr <= accessprops_raddr >> 3;
                accessprops_position <= accessprops_raddr[2:0];
                request_state <= STATE_RECEIVE_ACCESSPROPS;
            end

            STATE_RECEIVE_ACCESSPROPS:
            begin
                if (MEM_accessprops_read.rvalid)
                begin
                    accessprops_DRAM_offset <= MEM_accessprops_read.rdata[accessprops_position*64+31 -: 32];
                    accessprops_DRAM_length <= MEM_accessprops_read.rdata[accessprops_position*64+63 -: 32];
                    request_state <= STATE_ADD_ACCESSPROPS;
                end
            end

            STATE_ADD_ACCESSPROPS:
            begin
                running_DRAM_load_offset <= running_DRAM_load_offset + accessprops_DRAM_offset;
                DRAM_load_length <= accessprops_DRAM_length;
                REGION0_accessproperties[29:16] <= accessprops_DRAM_length[13:0];
                REGION1_accessproperties[29:16] <= accessprops_DRAM_length[13:0];
                REGION2_accessproperties[29:16] <= accessprops_DRAM_length[13:0];
                MEM_accessprops_accessproperties[29:16] <= accessprops_DRAM_length[13:0];
                MEM_localprops_accessproperties[29:16] <= accessprops_DRAM_length[13:0];
                REGION_prefetch_accessproperties[29:16] <= accessprops_DRAM_length[13:0];
                request_state <= trigger_dma ? STATE_DMA_TRIGGER : STATE_WRITE_TRIGGER;
            end

            STATE_DMA_TRIGGER:
            begin
                if (DRAM_load_length == 0)
                begin
                    request_state <= STATE_DONE;
                end
                else if (DMA_read.status.idle)
                begin
                    DMA_read.control.start <= 1'b1;
                    DMA_read.control.regs <= t_dma_reg'(0);
                    DMA_read.control.regs.reg4 <= {enable_multiline, DRAM_load_length};
                    DMA_read.control.addr <= {upper_part_address, running_DRAM_load_offset};
                    DMA_read.control.async <= 1'b0;
                    request_state <= STATE_WRITE_TRIGGER;
                end
            end

            STATE_WRITE_TRIGGER:
            begin
                write_trigger <= 1'b1;
                request_state <= STATE_READ;
            end

            STATE_READ:
            begin
                if (DMA_read.status.active && num_allowed_lines_to_request > 0)
                begin
                    if (enable_multiline && (num_requested_lines_plus4 < DRAM_load_length))
                    begin
                        DMA_read.tx_read.re <= 1'b1;
                        DMA_read.tx_read.rlength <= 2'b11;
                        num_requested_lines <= num_requested_lines_plus4;
                        num_requested_lines_plus2 <= num_requested_lines_plus4 + 2;
                        num_requested_lines_plus4 <= num_requested_lines_plus4 + 4;
                    end
                    else if (enable_multiline && 
                        (num_requested_lines_plus2 < DRAM_load_length))
                    begin
                        DMA_read.tx_read.re <= 1'b1;
                        DMA_read.tx_read.rlength <= 2'b01;
                        num_requested_lines <= num_requested_lines_plus2;
                        num_requested_lines_plus2 <= num_requested_lines_plus2 + 2;
                        num_requested_lines_plus4 <= num_requested_lines_plus2 + 4;
                    end
                    else if (num_requested_lines < DRAM_load_length)
                    begin
                        DMA_read.tx_read.re <= 1'b1;
                        DMA_read.tx_read.rlength <= 2'b00;
                        num_requested_lines <= num_requested_lines + 1;
                        num_requested_lines_plus2 <= num_requested_lines + 3;
                        num_requested_lines_plus4 <= num_requested_lines + 5;
                    end
                end

                if (prefetch_fifo_access.rvalid)
                begin
                    from_load.we <= 1'b1;
                    from_load.wdata <= prefetch_fifo_access.rdata;
                    num_forward_request_lines <= num_forward_request_lines + 1;
                end

                if (num_forward_request_lines == DRAM_load_length)
                begin
                    request_state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                op_done <= 1'b1;
                request_state <= STATE_IDLE;
            end
        endcase

        if (internal_reset)
        begin
            request_state <= STATE_IDLE;
        end

    end

endmodule // glm_load