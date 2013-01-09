library ieee;
use ieee.std_logic_1164.all;

entity FIR is
    generic (taps : integer_vector); 
    port (
        clk      : in  std_logic;
        sample   : in  integer;
        filtered : out integer := 0);
end entity FIR;

architecture a1 of FIR is
begin  -- architecture a1
    process (clk) is
        variable delay_line : integer_vector(0 to taps'length-1) := (others => 0);
        variable sum : integer;
    begin  -- process
        if rising_edge(clk) then  -- rising clock edge
            delay_line := sample & delay_line(0 to taps'length-2);
            sum := 0;
            for i in 0 to taps'length-1 loop
                sum := sum + delay_line(i)*taps(taps'high-i);
            end loop;
            filtered <= sum;
        end if;
    end process;
end architecture a1;