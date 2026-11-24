%% Plot EnergyTrace data: LPM0 vs LPM3
% Export each EnergyTrace session from Code Composer Studio as CSV
% (EnergyTrace window -> Save raw data), then set the file names below.
%
% Expected CSV columns (CCS EnergyTrace export):
%   Time (ns), Current (nA), Voltage (mV), Energy (uJ)

clear; clc; close all;

files  = {'energytrace_lpm0.csv', 'energytrace_lpm3.csv'};
labels = {'LPM0', 'LPM3'};
colors = {'b', 'r'};

figure('Name', 'MSP432 EnergyTrace: LPM0 vs LPM3');

for k = 1:numel(files)
    T = readmatrix(files{k});
    t_s    = T(:,1) * 1e-9;               % ns  -> s
    i_mA   = T(:,2) * 1e-6;               % nA  -> mA
    v_V    = T(:,3) * 1e-3;               % mV  -> V
    e_mJ   = T(:,4) * 1e-3;               % uJ  -> mJ
    p_mW   = i_mA .* v_V;                 % mW

    subplot(2,1,1); hold on;
    plot(t_s, e_mJ, colors{k}, 'DisplayName', labels{k});

    subplot(2,1,2); hold on;
    plot(t_s, p_mW, colors{k}, 'DisplayName', labels{k});

    fprintf('%s: mean power = %.4f mW, mean current = %.4f mA, energy = %.3f mJ\n', ...
            labels{k}, mean(p_mW), mean(i_mA), e_mJ(end) - e_mJ(1));
end

subplot(2,1,1); grid on; legend show;
xlabel('Time (s)'); ylabel('Energy (mJ)'); title('Cumulative energy');

subplot(2,1,2); grid on; legend show;
xlabel('Time (s)'); ylabel('Power (mW)');   title('Power consumption');
