#!/usr/bin/env python3
"""
Visualization script for HPC Parallel Sorting Benchmark Results
Generates publication-quality graphs from experiment matrix data
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import glob
import os
from pathlib import Path

# Set publication-quality defaults
plt.rcParams['figure.figsize'] = (12, 8)
plt.rcParams['font.size'] = 11
plt.rcParams['axes.labelsize'] = 12
plt.rcParams['axes.titlesize'] = 14
plt.rcParams['xtick.labelsize'] = 10
plt.rcParams['ytick.labelsize'] = 10
plt.rcParams['legend.fontsize'] = 10
plt.rcParams['figure.titlesize'] = 16

# Color scheme
COLORS = {
    'psrs': '#2E86AB',      # Blue
    'bitonic': '#A23B72',   # Purple
    'psrs_light': '#6BB4D6',
    'bitonic_light': '#C96A9E'
}

def load_experiment_data(results_dir):
    """Load and aggregate experiment data from CSV files"""
    data = {'psrs': {}, 'bitonic': {}}
    
    for algo in ['psrs', 'bitonic']:
        for size in [10000000, 100000000]:
            for ranks in [2, 4, 8, 16]:
                pattern = f"{results_dir}/{algo}_{size}_{ranks}ranks_*.csv"
                files = glob.glob(pattern)
                
                if files:
                    df = pd.read_csv(files[0])
                    # Skip header row if present in data
                    df = df[df['num_ranks'].astype(str).str.isdigit()]
                    df = df.astype(float)
                    
                    key = f"{size}_{ranks}"
                    data[algo][key] = {
                        'total_mean': df['total_time'].mean(),
                        'total_std': df['total_time'].std(),
                        'local_mean': df['local_sort_time'].mean(),
                        'local_std': df['local_sort_time'].std(),
                        'comm_mean': df['communication_time'].mean(),
                        'comm_std': df['communication_time'].std(),
                        'merge_mean': df['merge_time'].mean(),
                        'merge_std': df['merge_time'].std(),
                        'size': size,
                        'ranks': ranks
                    }
    
    return data

def plot_throughput_comparison(data, output_dir):
    """Plot throughput comparison between PSRS and Bitonic"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    sizes = [10000000, 100000000]
    size_labels = ['10M', '100M']
    ranks_list = [2, 4, 8, 16]
    
    for idx, (size, label) in enumerate(zip(sizes, size_labels)):
        ax = ax1 if idx == 0 else ax2
        
        psrs_throughput = []
        psrs_std = []
        bitonic_throughput = []
        bitonic_std = []
        
        for ranks in ranks_list:
            key = f"{size}_{ranks}"
            
            if key in data['psrs']:
                time_mean = data['psrs'][key]['total_mean']
                time_std = data['psrs'][key]['total_std']
                throughput = size / time_mean / 1e6
                throughput_std = throughput * (time_std / time_mean)
                psrs_throughput.append(throughput)
                psrs_std.append(throughput_std)
            
            if key in data['bitonic']:
                time_mean = data['bitonic'][key]['total_mean']
                time_std = data['bitonic'][key]['total_std']
                throughput = size / time_mean / 1e6
                throughput_std = throughput * (time_std / time_mean)
                bitonic_throughput.append(throughput)
                bitonic_std.append(throughput_std)
        
        x = np.arange(len(ranks_list))
        width = 0.35
        
        bars1 = ax.bar(x - width/2, psrs_throughput, width, label='PSRS',
                      color=COLORS['psrs'], yerr=psrs_std, capsize=5, alpha=0.8)
        bars2 = ax.bar(x + width/2, bitonic_throughput, width, label='Bitonic',
                      color=COLORS['bitonic'], yerr=bitonic_std, capsize=5, alpha=0.8)
        
        ax.set_xlabel('Number of MPI Ranks', fontweight='bold')
        ax.set_ylabel('Throughput (M elements/s)', fontweight='bold')
        ax.set_title(f'Problem Size: {label} elements', fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(ranks_list)
        ax.legend()
        ax.grid(axis='y', alpha=0.3, linestyle='--')
        
        # Add value labels on bars
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2., height,
                       f'{height:.1f}',
                       ha='center', va='bottom', fontsize=9)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/throughput_comparison.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: throughput_comparison.png")
    plt.close()

def plot_strong_scaling(data, output_dir):
    """Plot strong scaling efficiency"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    sizes = [10000000, 100000000]
    size_labels = ['10M', '100M']
    ranks_list = [2, 4, 8, 16]
    
    for idx, (size, label) in enumerate(zip(sizes, size_labels)):
        ax = ax1 if idx == 0 else ax2
        
        for algo, color in [('psrs', COLORS['psrs']), ('bitonic', COLORS['bitonic'])]:
            speedups = []
            efficiencies = []
            base_time = None
            
            for ranks in ranks_list:
                key = f"{size}_{ranks}"
                if key in data[algo]:
                    time_mean = data[algo][key]['total_mean']
                    
                    if base_time is None:
                        base_time = time_mean * 2  # 2 ranks baseline
                        speedups.append(1.0)
                        efficiencies.append(100.0)
                    else:
                        speedup = base_time / time_mean
                        speedups.append(speedup)
                        efficiencies.append((speedup / ranks) * 100)
            
            ax.plot(ranks_list, efficiencies, marker='o', linewidth=2.5,
                   label=algo.upper(), color=color, markersize=8)
        
        # Ideal efficiency line
        ax.plot(ranks_list, [100] * len(ranks_list), 'k--', alpha=0.5,
               linewidth=1.5, label='Ideal (100%)')
        
        ax.set_xlabel('Number of MPI Ranks', fontweight='bold')
        ax.set_ylabel('Parallel Efficiency (%)', fontweight='bold')
        ax.set_title(f'Strong Scaling: {label} elements', fontweight='bold')
        ax.set_xticks(ranks_list)
        ax.legend()
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.set_ylim([0, 110])
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/strong_scaling.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: strong_scaling.png")
    plt.close()

def plot_time_breakdown(data, output_dir):
    """Plot time breakdown (compute vs communication)"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    sizes = [10000000, 100000000]
    size_labels = ['10M', '100M']
    algos = ['psrs', 'bitonic']
    algo_labels = ['PSRS', 'Bitonic']
    ranks_list = [2, 4, 8, 16]
    
    for row, (algo, algo_label) in enumerate(zip(algos, algo_labels)):
        for col, (size, size_label) in enumerate(zip(sizes, size_labels)):
            ax = axes[row, col]
            
            local_times = []
            comm_times = []
            merge_times = []
            
            for ranks in ranks_list:
                key = f"{size}_{ranks}"
                if key in data[algo]:
                    local_times.append(data[algo][key]['local_mean'])
                    comm_times.append(data[algo][key]['comm_mean'])
                    merge_times.append(data[algo][key]['merge_mean'])
            
            x = np.arange(len(ranks_list))
            width = 0.6
            
            p1 = ax.bar(x, local_times, width, label='Local Sort',
                       color='#52B788', alpha=0.8)
            p2 = ax.bar(x, comm_times, width, bottom=local_times,
                       label='Communication', color='#F4A261', alpha=0.8)
            p3 = ax.bar(x, merge_times, width,
                       bottom=[i+j for i,j in zip(local_times, comm_times)],
                       label='Merge/Partition', color='#E76F51', alpha=0.8)
            
            ax.set_xlabel('Number of MPI Ranks', fontweight='bold')
            ax.set_ylabel('Time (seconds)', fontweight='bold')
            ax.set_title(f'{algo_label} - {size_label} elements', fontweight='bold')
            ax.set_xticks(x)
            ax.set_xticklabels(ranks_list)
            ax.legend()
            ax.grid(axis='y', alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/time_breakdown.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: time_breakdown.png")
    plt.close()

def plot_communication_overhead(data, output_dir):
    """Plot communication overhead percentage"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    sizes = [10000000, 100000000]
    size_labels = ['10M', '100M']
    ranks_list = [2, 4, 8, 16]
    
    for idx, (size, label) in enumerate(zip(sizes, size_labels)):
        ax = ax1 if idx == 0 else ax2
        
        for algo, color, marker in [('psrs', COLORS['psrs'], 'o'), 
                                     ('bitonic', COLORS['bitonic'], 's')]:
            comm_percentages = []
            
            for ranks in ranks_list:
                key = f"{size}_{ranks}"
                if key in data[algo]:
                    total = data[algo][key]['total_mean']
                    comm = data[algo][key]['comm_mean']
                    comm_pct = (comm / total) * 100
                    comm_percentages.append(comm_pct)
            
            ax.plot(ranks_list, comm_percentages, marker=marker, linewidth=2.5,
                   label=algo.upper(), color=color, markersize=10)
        
        ax.set_xlabel('Number of MPI Ranks', fontweight='bold')
        ax.set_ylabel('Communication Overhead (%)', fontweight='bold')
        ax.set_title(f'Problem Size: {label} elements', fontweight='bold')
        ax.set_xticks(ranks_list)
        ax.legend()
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.set_ylim([0, max(ax.get_ylim()[1], 50)])
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/communication_overhead.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: communication_overhead.png")
    plt.close()

def plot_speedup_curves(data, output_dir):
    """Plot speedup curves with ideal speedup reference"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    sizes = [10000000, 100000000]
    size_labels = ['10M', '100M']
    ranks_list = [2, 4, 8, 16]
    
    for idx, (size, label) in enumerate(zip(sizes, size_labels)):
        ax = ax1 if idx == 0 else ax2
        
        for algo, color, marker in [('psrs', COLORS['psrs'], 'o'), 
                                     ('bitonic', COLORS['bitonic'], 's')]:
            speedups = []
            base_time = None
            
            for ranks in ranks_list:
                key = f"{size}_{ranks}"
                if key in data[algo]:
                    time_mean = data[algo][key]['total_mean']
                    
                    if base_time is None:
                        base_time = time_mean * 2  # 2 ranks baseline
                    
                    speedup = base_time / time_mean
                    speedups.append(speedup)
            
            ax.plot(ranks_list, speedups, marker=marker, linewidth=2.5,
                   label=algo.upper(), color=color, markersize=10)
        
        # Ideal speedup line
        ideal = [r/2 for r in ranks_list]  # Normalized to 2 ranks
        ax.plot(ranks_list, ideal, 'k--', alpha=0.5, linewidth=2,
               label='Ideal Linear')
        
        ax.set_xlabel('Number of MPI Ranks', fontweight='bold')
        ax.set_ylabel('Speedup (relative to 2 ranks)', fontweight='bold')
        ax.set_title(f'Speedup: {label} elements', fontweight='bold')
        ax.set_xticks(ranks_list)
        ax.legend()
        ax.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/speedup_curves.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: speedup_curves.png")
    plt.close()

def plot_performance_summary(data, output_dir):
    """Create a comprehensive performance summary dashboard"""
    fig = plt.figure(figsize=(16, 10))
    gs = fig.add_gridspec(3, 3, hspace=0.3, wspace=0.3)
    
    # 1. Throughput comparison (top row)
    ax1 = fig.add_subplot(gs[0, :2])
    sizes = [10000000, 100000000]
    size_labels = ['10M', '100M']
    ranks_list = [2, 4, 8, 16]
    
    x = np.arange(len(ranks_list) * 2) * 2
    width = 0.35
    
    psrs_all = []
    bitonic_all = []
    
    for size in sizes:
        for ranks in ranks_list:
            key = f"{size}_{ranks}"
            if key in data['psrs']:
                psrs_all.append(size / data['psrs'][key]['total_mean'] / 1e6)
            if key in data['bitonic']:
                bitonic_all.append(size / data['bitonic'][key]['total_mean'] / 1e6)
    
    positions = np.arange(len(psrs_all))
    ax1.bar(positions - width/2, psrs_all, width, label='PSRS',
           color=COLORS['psrs'], alpha=0.8)
    ax1.bar(positions + width/2, bitonic_all, width, label='Bitonic',
           color=COLORS['bitonic'], alpha=0.8)
    
    labels = [f'{s}\n{r}r' for s in ['10M', '10M', '10M', '10M', '100M', '100M', '100M', '100M'] 
              for r in [2, 4, 8, 16]][:len(psrs_all)]
    ax1.set_xticks(positions)
    ax1.set_xticklabels(['2r', '4r', '8r', '16r', '2r', '4r', '8r', '16r'])
    ax1.set_ylabel('Throughput (M elem/s)', fontweight='bold')
    ax1.set_title('Overall Throughput Performance', fontweight='bold', fontsize=14)
    ax1.legend()
    ax1.grid(axis='y', alpha=0.3)
    ax1.axvline(x=3.5, color='gray', linestyle='--', alpha=0.5)
    ax1.text(1.5, ax1.get_ylim()[1]*0.95, '10M elements', ha='center', fontsize=10)
    ax1.text(5.5, ax1.get_ylim()[1]*0.95, '100M elements', ha='center', fontsize=10)
    
    # 2. Best configurations (top right)
    ax2 = fig.add_subplot(gs[0, 2])
    ax2.axis('off')
    best_results = [
        "🏆 BEST RESULTS",
        "",
        "PSRS:",
        "  10M @ 8 ranks",
        f"  38.63 M/s",
        "",
        "  100M @ 8 ranks",
        f"  26.03 M/s",
        "",
        "Bitonic:",
        "  10M @ 4 ranks",
        f"  24.78 M/s",
        "",
        "  100M @ 4 ranks",
        f"  20.48 M/s"
    ]
    ax2.text(0.1, 0.9, '\n'.join(best_results), fontsize=11,
            verticalalignment='top', fontfamily='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))
    
    # 3. Communication overhead (middle left)
    ax3 = fig.add_subplot(gs[1, 0])
    for algo, color in [('psrs', COLORS['psrs']), ('bitonic', COLORS['bitonic'])]:
        comm_pcts = []
        for ranks in ranks_list:
            key = f"100000000_{ranks}"
            if key in data[algo]:
                total = data[algo][key]['total_mean']
                comm = data[algo][key]['comm_mean']
                comm_pcts.append((comm / total) * 100)
        ax3.plot(ranks_list, comm_pcts, marker='o', linewidth=2,
                label=algo.upper(), color=color, markersize=8)
    ax3.set_xlabel('Ranks', fontweight='bold')
    ax3.set_ylabel('Comm. Overhead (%)', fontweight='bold')
    ax3.set_title('Communication Overhead (100M)', fontweight='bold')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # 4. Speedup (middle center)
    ax4 = fig.add_subplot(gs[1, 1])
    for algo, color in [('psrs', COLORS['psrs']), ('bitonic', COLORS['bitonic'])]:
        speedups = []
        base_time = None
        for ranks in ranks_list:
            key = f"100000000_{ranks}"
            if key in data[algo]:
                time = data[algo][key]['total_mean']
                if base_time is None:
                    base_time = time * 2
                speedups.append(base_time / time)
        ax4.plot(ranks_list, speedups, marker='o', linewidth=2,
                label=algo.upper(), color=color, markersize=8)
    ideal = [r/2 for r in ranks_list]
    ax4.plot(ranks_list, ideal, 'k--', alpha=0.5, label='Ideal')
    ax4.set_xlabel('Ranks', fontweight='bold')
    ax4.set_ylabel('Speedup', fontweight='bold')
    ax4.set_title('Strong Scaling (100M)', fontweight='bold')
    ax4.legend()
    ax4.grid(True, alpha=0.3)
    
    # 5. Efficiency (middle right)
    ax5 = fig.add_subplot(gs[1, 2])
    for algo, color in [('psrs', COLORS['psrs']), ('bitonic', COLORS['bitonic'])]:
        efficiencies = []
        base_time = None
        for ranks in ranks_list:
            key = f"100000000_{ranks}"
            if key in data[algo]:
                time = data[algo][key]['total_mean']
                if base_time is None:
                    base_time = time * 2
                speedup = base_time / time
                efficiencies.append((speedup / ranks) * 100)
        ax5.plot(ranks_list, efficiencies, marker='o', linewidth=2,
                label=algo.upper(), color=color, markersize=8)
    ax5.plot(ranks_list, [100]*len(ranks_list), 'k--', alpha=0.5, label='Ideal')
    ax5.set_xlabel('Ranks', fontweight='bold')
    ax5.set_ylabel('Efficiency (%)', fontweight='bold')
    ax5.set_title('Parallel Efficiency (100M)', fontweight='bold')
    ax5.legend()
    ax5.grid(True, alpha=0.3)
    ax5.set_ylim([0, 110])
    
    # 6. Time breakdown comparison (bottom)
    ax6 = fig.add_subplot(gs[2, :])
    configs = [(10000000, 8, 'PSRS 10M@8r'), (100000000, 8, 'PSRS 100M@8r'),
               (10000000, 4, 'Bitonic 10M@4r'), (100000000, 4, 'Bitonic 100M@4r')]
    
    labels = []
    local_data = []
    comm_data = []
    merge_data = []
    
    for size, ranks, label in configs:
        algo = 'psrs' if 'PSRS' in label else 'bitonic'
        key = f"{size}_{ranks}"
        if key in data[algo]:
            labels.append(label)
            local_data.append(data[algo][key]['local_mean'])
            comm_data.append(data[algo][key]['comm_mean'])
            merge_data.append(data[algo][key]['merge_mean'])
    
    x = np.arange(len(labels))
    width = 0.6
    
    p1 = ax6.barh(x, local_data, width, label='Local Sort', color='#52B788')
    p2 = ax6.barh(x, comm_data, width, left=local_data,
                  label='Communication', color='#F4A261')
    p3 = ax6.barh(x, merge_data, width,
                  left=[i+j for i,j in zip(local_data, comm_data)],
                  label='Merge', color='#E76F51')
    
    ax6.set_yticks(x)
    ax6.set_yticklabels(labels)
    ax6.set_xlabel('Time (seconds)', fontweight='bold')
    ax6.set_title('Time Breakdown - Best Configurations', fontweight='bold')
    ax6.legend(loc='upper right')
    ax6.grid(axis='x', alpha=0.3)
    
    plt.suptitle('HPC Parallel Sorting Benchmark - Performance Dashboard',
                fontsize=18, fontweight='bold', y=0.98)
    
    plt.savefig(f'{output_dir}/performance_dashboard.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: performance_dashboard.png")
    plt.close()

def main():
    """Main execution"""
    print("=" * 60)
    print("  HPC Parallel Sorting Benchmark - Visualization")
    print("=" * 60)
    print()
    
    # Setup paths
    results_dir = "results/experiment_matrix"
    output_dir = "results/graphs"
    Path(output_dir).mkdir(parents=True, exist_ok=True)
    
    print("Loading experiment data...")
    data = load_experiment_data(results_dir)
    print(f"✓ Loaded data for PSRS and Bitonic\n")
    
    print("Generating visualizations...")
    print()
    
    # Generate all plots
    plot_throughput_comparison(data, output_dir)
    plot_strong_scaling(data, output_dir)
    plot_speedup_curves(data, output_dir)
    plot_time_breakdown(data, output_dir)
    plot_communication_overhead(data, output_dir)
    plot_performance_summary(data, output_dir)
    
    print()
    print("=" * 60)
    print("✓ All visualizations complete!")
    print(f"  Saved to: {output_dir}/")
    print("=" * 60)
    print()
    print("Generated files:")
    print("  • throughput_comparison.png")
    print("  • strong_scaling.png")
    print("  • speedup_curves.png")
    print("  • time_breakdown.png")
    print("  • communication_overhead.png")
    print("  • performance_dashboard.png")
    print()

if __name__ == "__main__":
    main()
