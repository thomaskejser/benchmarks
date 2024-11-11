import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import sys
from matplotlib.ticker import MaxNLocator, FuncFormatter

languages = {"python": "#306998",
             "c#": "#68217A",
             "c++": "#007ACC"
             }


def read_data(file_name: str) :
    data = pd.read_csv(file_name, sep="|")
    data["ops_per_sec"] = pd.to_numeric(data["ops_per_sec"], errors='coerce') / 1_000_000
    print(data)
    return data

def create_bar_chart(data, output_file: str):

    my_languages = {k : v for k,v in languages.items() if k in data["language"].unique()}

    fig, ax = plt.subplots(figsize=(6, 12))
    bar_width = 3.0 / len(my_languages)
    group_spacing = bar_width * len(my_languages) * 1.2
    test_map = {name: i * group_spacing for i, name in enumerate(data['test'].unique())}


    for i, language in enumerate(my_languages.keys()):
        lang = data[data['language'] == language]
        offset = ((len(my_languages) - 1) * bar_width) / 2
        # - len(my_languages) / bar_width +
        bars = ax.bar(
                [test_map[name] + (bar_width * i) - offset for name in lang['test']],
                lang["ops_per_sec"],
                color=languages[language],
                width=bar_width)

    for bar, label in zip(bars, lang['language']):
        ax.text(
                bar.get_x() + bar.get_width() / 2,
                bar.get_height() - 0.9,
                label,
                ha='center',
                va='top',
                color="white"
        )

    ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda x, _: f'{x / 1e6:.0f}M'))
    ax.set_xticks(list(test_map.values()))
    ax.set_xticklabels(list(test_map.keys()))
    plt.xticks(rotation=45, ha='right')
    plt.yticks(range(0,
            int(data['ops_per_sec'].max()) + 2),
            labels=[f'{i}M' for i in range(0, int(data['ops_per_sec'].max()) + 2)])

    ax.set_title('Performance')
    ax.set_xlabel('Test')
    ax.set_ylabel('ops/sec')

    fig.tight_layout()
    plt.savefig(output_file, dpi=100)
    plt.show()
    plt.close()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <file_name.csv>")
    else:
        file_name = sys.argv[1]
        data = read_data(file_name)

        create_bar_chart(data, "all.png")

        # JSON for all languages
        create_bar_chart(data[data['test'].str.contains("single", case=False)], "single.png")
        # JSON experiments with algos
        create_bar_chart(data[data['test'].str.contains("JSON \(", case=False)], "json.png")
        # Process
        create_bar_chart(data[data['test'].str.contains("process", case=False)], "process.png")

