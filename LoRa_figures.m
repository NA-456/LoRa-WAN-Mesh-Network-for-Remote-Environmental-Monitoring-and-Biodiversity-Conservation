% =========================================================================
% IEEE Conference Figures Generator (Extended 1km Horizon & Terrain Model)
% Addresses Pre-Submission Review Issues C3, C4, and Section 4
% =========================================================================
clear; clc; close all;
set(0, 'DefaultTextFontName', 'Times New Roman');
set(0, 'DefaultAxesFontName', 'Times New Roman');
set(0, 'DefaultAxesFontSize', 9);
set(0, 'DefaultTextFontSize', 9);
set(0, 'DefaultLineLineWidth', 1.5);
set(0, 'DefaultAxesLineWidth', 1.0);
c_blue   = [0.00, 0.45, 0.74];
c_red    = [0.85, 0.33, 0.10];
c_green  = [0.15, 0.60, 0.20];
c_purple = [0.49, 0.18, 0.56];
% Single-column IEEE dimensions (cm)
fig_w = 8.8;
fig_h = 6.4;
%% =========================================================================
% 1. Figure 1: Attenuation vs Distance (Extended to 1000 m with Terrain Ridge)
% =========================================================================
d_m = linspace(10, 1000, 500); % Extended to 1 km per Review C3
d_km = d_m / 1000;
freq_mhz = 868;
freq_ghz = freq_mhz / 1000;
% Free Space Path Loss (FSPL)
fspl = 20*log10(d_km) + 20*log10(freq_mhz) + 32.44;
% Foliage Attenuation (Modified Weissberger: f in GHz, d_veg in meters)
foliage_depth_m = d_m * 0.75; % Dense equatorial forest canopy penetration
foliage_loss = zeros(size(d_m));
for i = 1:length(d_m)
    if foliage_depth_m(i) <= 14
        foliage_loss(i) = 1.33 * (freq_ghz^0.284) * (foliage_depth_m(i)^0.588);
    else
        foliage_loss(i) = 0.45 * (freq_ghz^0.284) * foliage_depth_m(i);
    end
end
total_foliage_loss = fspl + foliage_loss;
% Additional Knife-Edge Ridge Diffraction Loss for Obstructed Basin Links
% Models an intermediate terrain crest obstructing direct line of sight beyond 250 m
ridge_diffraction = zeros(size(d_m));
ridge_diffraction(d_m >= 250) = 18.5 + 4.5 * log10(d_m(d_m >= 250) / 250);
total_obstructed_loss = total_foliage_loss + ridge_diffraction;
fig1 = figure('Units', 'centimeters', 'Position', [1, 1, fig_w, fig_h], 'Color', 'w');
plot(d_m, fspl, '--', 'Color', [0.45 0.45 0.45], 'LineWidth', 1.2, 'DisplayName', 'FSPL (Free Space)');
hold on; grid on; box on;
plot(d_m, total_foliage_loss, '-', 'Color', c_green, 'LineWidth', 1.6, 'DisplayName', 'FSPL + Wet Canopy');
plot(d_m, total_obstructed_loss, '-.', 'Color', c_purple, 'LineWidth', 1.6, 'DisplayName', 'Canopy + Terrain Ridge');
% Maximum path loss thresholds: Tx = +14 dBm, G_tx + G_rx = 4.3 dBi
% SF7 sensitivity = -125 dBm -> Max allowable path loss = 14 + 4.3 - (-125) = 143.3 dB
% SF12 sensitivity = -137 dBm -> Max allowable path loss = 14 + 4.3 - (-137) = 155.3 dB
yline(143.3, ':b', 'SF7 Cutoff (143.3 dB)', 'LineWidth', 1.1, 'LabelVerticalAlignment', 'bottom', 'FontSize', 7.5);
yline(155.3, ':k', 'SF12 Cutoff (155.3 dB)', 'LineWidth', 1.1, 'LabelVerticalAlignment', 'bottom', 'FontSize', 7.5);
xlabel('Transmission Distance (m)', 'FontWeight', 'bold');
ylabel('Total Path Loss (dB)', 'FontWeight', 'bold');
xlim([10, 1000]);
ylim([50, 170]);
legend('Location', 'northwest', 'FontSize', 7);
set(fig1, 'PaperUnits', 'centimeters', 'PaperPosition', [0 0 fig_w fig_h], 'PaperSize', [fig_w fig_h]);
print(fig1, 'Fig3_Path_Loss_Canopy.pdf', '-dpdf', '-vector');
print(fig1, 'Fig3_Path_Loss_Canopy.png', '-dpng', '-r600');
%% =========================================================================
% 2. Figure 2: Latency vs Inter-Node Distance (Corrected In-Chart Labels)
% =========================================================================
dist_sweep_m   = [10, 15, 25, 35, 45, 60];
delay_dist_ms  = [0.0038000, 0.0038082, 0.0038083, 0.0290117, 0.0299590, 0.0752968] * 1000;
fig2 = figure('Units', 'centimeters', 'Position', [2, 2, fig_w, fig_h], 'Color', 'w');
plot(dist_sweep_m, delay_dist_ms, '-o', 'Color', c_blue, 'MarkerFaceColor', c_blue, ...
    'MarkerSize', 4.5, 'LineWidth', 1.5, 'DisplayName', '5-Node Chain (Swept Spacing)');
grid on; box on;
xlabel('Inter-Node Spacing (m)', 'FontWeight', 'bold');
ylabel('End-to-End Latency (ms)', 'FontWeight', 'bold');
xlim([5, 65]); ylim([0, 85]);
legend('Location', 'northwest', 'FontSize', 7.5);
set(fig2, 'PaperUnits', 'centimeters', 'PaperPosition', [0 0 fig_w fig_h], 'PaperSize', [fig_w fig_h]);
print(fig2, 'Fig1_Latency_vs_Distance.pdf', '-dpdf', '-vector');
print(fig2, 'Fig1_Latency_vs_Distance.png', '-dpng', '-r600');
%% =========================================================================
% 3. Figure 3: Scalability Latency vs Node Count (Corrected In-Chart Labels)
% =========================================================================
nodes_count    = [5, 10, 15, 20];
delay_nodes_ms = [0.0038082, 0.0267844, 0.0386375, 0.0526537] * 1000;
fig3 = figure('Units', 'centimeters', 'Position', [3, 3, fig_w, fig_h], 'Color', 'w');
plot(nodes_count, delay_nodes_ms, '-s', 'Color', c_red, 'MarkerFaceColor', c_red, ...
    'MarkerSize', 4.5, 'LineWidth', 1.5, 'DisplayName', 'Fixed 15 m Spacing (Swept Nodes)');
grid on; box on;
xlabel('Total Number of Nodes (N)', 'FontWeight', 'bold');
ylabel('End-to-End Latency (ms)', 'FontWeight', 'bold');
xticks(nodes_count); xlim([4, 21]); ylim([0, 60]);
legend('Location', 'northwest', 'FontSize', 7.5);
set(fig3, 'PaperUnits', 'centimeters', 'PaperPosition', [0 0 fig_w fig_h], 'PaperSize', [fig_w fig_h]);
print(fig3, 'Fig2_Scaling_Latency.pdf', '-dpdf', '-vector');
print(fig3, 'Fig2_Scaling_Latency.png', '-dpng', '-r600');
disp('Regeneration complete: Extended 1 km attenuation profile and corrected latency figures saved.');
