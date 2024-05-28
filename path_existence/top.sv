`default_nettype none
module top (input wire clk, rstn, din, output logic q);
  logic [3:0] tmp;
	  always_ff @(posedge clk) begin
		  if(!rstn) tmp <= '0;
		  else begin
			  for(int i = 0; i < 4; i++) begin
				  if(i == 0) tmp[i] <= din;
				  else tmp[i] <= tmp[i-1];
	                  end
	         end
	end
  assign q = tmp[3];
endmodule
`default_nettype wire
