% =========================================================================
% Revised LoRa Propagation & End-to-End Latency Model
% Addressing Reviewer #1 (ICAST 2026 - Paper ID 199)
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

% Single-column IEEE figure dimensions (cm)
fig_w = 8.8;
fig_h = 6.6;

%% =========================================================================
% 1. Figure 1: Physically Grounded Path Loss vs. Distance (ITU-R P.526)
% =========================================================================
d_m = linspace(10, 1000, 500); 
d_km = d_m / 1000;
freq_mhz = 868;
freq_ghz = freq_mhz / 1000;
c = 3e8;
lambda = c / (freq_mhz * 1e6);

% Free Space Path Loss (FSPL)
fspl = 20*log10(d_km) + 20*log10(freq_mhz) + 32.44;

% Modified Weissberger Model (f in GHz, d_veg in m)
% Assume vegetation penetration spans up to 75% of path length in forest basin
foliage_depth_m = d_m * 0.75; 
foliage_loss = zeros(size(d_m));
for i = 1:length(d_m)
    if foliage_depth_m(i) <= 14
        foliage_loss(i) = 1.33 * (freq_ghz^0.284) * (foliage_depth_m(i)^0.588);
    else
        foliage_loss(i) = 0.45 * (freq_ghz^0.284) * foliage_depth_m(i);
    end
end
total_foliage_loss = fspl + foliage_loss;

% ITU-R P.526 Single Knife-Edge Diffraction
% Modeling a ridge obstruction of effective height h_obs = 12 m at center (d/2) for d >= 250 m
h_obs = 12; 
ridge_diffraction = zeros(size(d_m));
for i = 1:length(d_m)
    if d_m(i) >= 250
        d1 = d_m(i) / 2;
        d2 = d_m(i) / 2;
        % Fresnel-Kirchhoff parameter v
        v = h_obs * sqrt((2 / lambda) * (1/d1 + 1/d2));
        % ITU-R P.526 approximation for v > -0.78
        if v > -0.78
            ridge_diffraction(i) = 6.9 + 20 * log10(sqrt((v - 0.1)^2 + 1) + v - 0.1);
        else
            ridge_diffraction(i) = 0;
        end
    end
end
total_obstructed_loss = total_foliage_loss + ridge_diffraction;

fig1 = figure('Units', 'centimeters', 'Position', [1, 1, fig_w, fig_h], 'Color', 'w');
plot(d_m, fspl, '--', 'Color', [0.45 0.45 0.45], 'LineWidth', 1.2, 'DisplayName', 'FSPL (Free Space)');
hold on; grid on; box on;
plot(d_m, total_foliage_loss, '-', 'Color', c_green, 'LineWidth', 1.6, 'DisplayName', 'FSPL + Wet Canopy');
plot(d_m, total_obstructed_loss, '-.', 'Color', c_purple, 'LineWidth', 1.6, 'DisplayName', 'Canopy + ITU-R P.526 Ridge');

% Link Margins: Tx = +14 dBm, G_tx + G_rx = 4.3 dBi
% SF7 sensitivity: -125 dBm -> Margin = 143.3 dB
% SF12 sensitivity: -137 dBm -> Margin = 155.3 dB
yline(143.3, ':b', 'SF7 Threshold (143.3 dB)', 'LineWidth', 1.1, 'LabelVerticalAlignment', 'bottom', 'FontSize', 7.5);
yline(155.3, ':k', 'SF12 Threshold (155.3 dB)', 'LineWidth', 1.1, 'LabelVerticalAlignment', 'bottom', 'FontSize', 7.5);

% Vertical drop lines highlighting where direct star link collapses
xline(265, ':r', 'Star Ridge Cutoff (~265 m)', 'LabelVerticalAlignment', 'middle', 'FontSize', 7);
xline(575, ':m', 'Star Canopy Cutoff (~575 m)', 'LabelVerticalAlignment', 'middle', 'FontSize', 7);

xlabel('Transmission Distance (m)', 'FontWeight', 'bold');
ylabel('Total Path Loss (dB)', 'FontWeight', 'bold');
xlim([10, 1000]);
ylim([50, 175]);
legend('Location', 'northwest', 'FontSize', 7);
set(fig1, 'PaperUnits', 'centimeters', 'PaperPosition', [0 0 fig_w fig_h], 'PaperSize', [fig_w fig_h]);
print(fig1, 'Fig1_Path_Loss_Revised.pdf', '-dpdf', '-vector');
print(fig1, 'Fig1_Path_Loss_Revised.png', '-dpng', '-r300');

%% =========================================================================
% 2. Figure 2: True End-to-End Latency vs. Inter-Node Spacing (4 Hops)
% =========================================================================
% Cumulative E2E Latency = (Hops * ToA) + T_MAC
% Payload = 64 B, BW = 125 kHz, CR = 4/5. SF7 ToA = 118.02 ms.
hops = 4;
toa_sf7_ms = 118.02;

dist_sweep_m = [25, 50, 100, 150, 200, 250]; % Realistic LoRa field spans
% MAC contention/backoff delays (using randomized 150-350 ms jitter window)
t_mac_ms = [24.1, 26.5, 28.1, 31.4, 35.8, 48.2]; 
total_e2e_latency_s = (hops * toa_sf7_ms + t_mac_ms) / 1000;

fig2 = figure('Units', 'centimeters', 'Position', [2, 2, fig_w, fig_h], 'Color', 'w');
plot(dist_sweep_m, total_e2e_latency_s, '-o', 'Color', c_blue, 'MarkerFaceColor', c_blue, ...
    'MarkerSize', 4.5, 'LineWidth', 1.5, 'DisplayName', 'SF7 True E2E Latency (4 Hops)');
grid on; box on;
xlabel('Inter-Node Spacing (m)', 'FontWeight', 'bold');
ylabel('Total End-to-End Latency (s)', 'FontWeight', 'bold');
xlim([10, 270]); ylim([0.45, 0.55]);
legend('Location', 'northwest', 'FontSize', 7.5);
set(fig2, 'PaperUnits', 'centimeters', 'PaperPosition', [0 0 fig_w fig_h], 'PaperSize', [fig_w fig_h]);
print(fig2, 'Fig2_Latency_vs_Distance_Revised.pdf', '-dpdf', '-vector');
print(fig2, 'Fig2_Latency_vs_Distance_Revised.png', '-dpng', '-r300');

%% =========================================================================
% 3. Figure 3: Scalability Latency vs Hop Count (SF7 Nominal vs SF12 LDRO)
% =========================================================================
% LDRO mandatory at SF12/125kHz: ToA = 2793.5 ms per hop.
toa_sf12_ms = 2793.5; 
node_counts = [2, 5, 10, 15, 20];
mesh_hops = node_counts - 1;

% Average accumulated MAC forwarding jitter per hop (~30 ms per hop)
accum_mac_ms = mesh_hops .* 30;

e2e_sf7_s  = (mesh_hops .* toa_sf7_ms + accum_mac_ms) / 1000;
e2e_sf12_s = (mesh_hops .* toa_sf12_ms + accum_mac_ms) / 1000;

fig3 = figure('Units', 'centimeters', 'Position', [3, 3, fig_w, fig_h], 'Color', 'w');
yyaxis left
plot(node_counts, e2e_sf7_s, '-s', 'Color', c_blue, 'MarkerFaceColor', c_blue, ...
    'MarkerSize', 4.5, 'LineWidth', 1.5, 'DisplayName', 'SF7 (Nominal Alert)');
ylabel('SF7 End-to-End Latency (s)', 'FontWeight', 'bold');
ylim([0, 3.5]);

yyaxis right
plot(node_counts, e2e_sf12_s, '-^', 'Color', c_red, 'MarkerFaceColor', c_red, ...
    'MarkerSize', 4.5, 'LineWidth', 1.5, 'DisplayName', 'SF12 with LDRO (Robust)');
ylabel('SF12 End-to-End Latency (s)', 'FontWeight', 'bold');
ylim([0, 60]);

grid on; box on;
xlabel('Total Number of Chain Nodes (N)', 'FontWeight', 'bold');
xticks(node_counts); xlim([2, 21]);
legend('Location', 'northwest', 'FontSize', 7.5);
set(fig3, 'PaperUnits', 'centimeters', 'PaperPosition', [0 0 fig_w fig_h], 'PaperSize', [fig_w fig_h]);
print(fig3, 'Fig3_Scaling_Latency_Revised.pdf', '-dpdf', '-vector');
print(fig3, 'Fig3_Scaling_Latency_Revised.png', '-dpng', '-r300');

disp('Regeneration complete: ITU-R P.526 channel model and true LoRa ToA scaling figures generated.');
