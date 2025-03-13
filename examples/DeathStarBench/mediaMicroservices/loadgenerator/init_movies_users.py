import aiohttp
import asyncio
import sys
import json
import argparse
import requests
import random
import string


def no_empty_fields(body):
    for val in body.values():
        if val is None:
            return False
        if type(val) is str and len(val) == 0:
            return False
    return True


def make_request(url, payload):
    # print(url, payload)
    if not no_empty_fields(payload):
        # print("not completing request due to empty fields in payload")
        return
    response = None
    status_code = 0
    while not response and status_code != 200:
        try:
            response = requests.post(url, data=payload)
            status_code = response.status_code
            # print(response)
        except requests.exceptions.ConnectionError:
            continue


def get_random_string(N):
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=N))


def init_register_users(addr):
    for i in range(1000):
        # registered_users.add(i)
        payload = {
            "first_name": "firstname_" + str(i),
            "last_name": "lastname_" + str(i),
            "username": "username_" + str(i),
            "password": "password_" + str(i),
        }
        make_request(url=addr + "/wrk2-api/user/register", payload=payload)


def init_register_movies(addr):
    for i in range(1000):
        # movie_title_len = random.randrange(10, 40)
        # title = get_random_string(movie_title_len)
        payload = {"title": "title_" +
                   str(i), "movie_id": "movie_id_" + str(i)}
        make_request(
            url=addr + "/wrk2-api/movie/register",
            payload=payload,
        )


async def upload_cast_info(session, addr, cast):
    success = False
    while not success:
        async with session.post(addr + "/wrk2-api/cast-info/write", json=cast) as resp:
            if resp.status == 200:
                success = True  # Exit loop on success
                return await resp.text()
            else:
                print(f"Attempt failed with status {resp.status}. Retrying...")


async def upload_plot(session, addr, plot):
    success = False
    while not success:
        async with session.post(addr + "/wrk2-api/plot/write", json=plot) as resp:
            if resp.status == 200:
                success = True  # Exit loop on success
                return await resp.text()
            else:
                print(f"Attempt failed with status {resp.status}. Retrying...")


async def upload_movie_info(session, addr, movie):
    success = False
    headers = {
        'Content-Type': 'application/json'
    }
    if len(movie['casts']) > 6:
        movie['casts'] = movie['casts'][:5]
    if movie['avg_rating'] == 0:
        movie['avg_rating'] = 1.1
    if movie['num_rating'] == 0:
        movie['num_rating'] = 1.1
    if movie['thumbnail_ids'] == [None]:
        movie['thumbnail_ids'] = ['/tNGJw2R1l3XuLRi749GQaFua9yZ.jpg']
    movie['avg_rating'] = str(movie['avg_rating'])
    movie['num_rating'] = str(movie['num_rating'])
    while not success:
        print("trying " + addr + "/wrk2-api/movie-info/write", movie)
        async with session.post(addr + "/wrk2-api/movie-info/write", json=movie, headers=headers) as resp:
            if resp.status == 200:
                success = True  # Exit loop on success
                return await resp.text()
            else:
                print(f"Attempt failed with status {resp.status}. Retrying...")

registered_movies = set()


async def register_movie(session, addr, movie):
    params = {
        "title": movie["title"],
        "movie_id": movie["movie_id"]
    }
    if movie["title"] in registered_movies:
        return
    registered_movies.add(movie["title"])
    success = False
    while not success:
        async with session.post(addr + "/wrk2-api/movie/register", data=params) as resp:
            if resp.status == 200:
                success = True  # Exit loop on success
                return await resp.text()
            else:
                print(f"Attempt failed with status {resp.status}. Retrying...")


async def write_cast_info(addr, raw_casts):
    idx = 0
    tasks = []
    conn = aiohttp.TCPConnector(limit=200)
    async with aiohttp.ClientSession(connector=conn) as session:
        for raw_cast in raw_casts:
            try:
                cast = dict()
                cast["cast_info_id"] = raw_cast["id"]
                cast["name"] = raw_cast["name"]
                cast["gender"] = True if raw_cast["gender"] == 2 else False
                cast["intro"] = raw_cast["biography"]
                task = asyncio.ensure_future(
                    upload_cast_info(session, addr, cast))
                tasks.append(task)
                idx += 1
            except:
                print("Warning: cast info missing!")
            if idx % 200 == 0:
                resps = await asyncio.gather(*tasks)
                print(idx, "casts finished")
        resps = await asyncio.gather(*tasks)
        print(idx, "casts finished")


async def write_movie_info(addr, raw_movies):
    idx = 0
    tasks = []
    conn = aiohttp.TCPConnector(limit=200)
    async with aiohttp.ClientSession(connector=conn) as session:
        for raw_movie in raw_movies:
            movie = dict()
            casts = list()
            movie["movie_id"] = str(raw_movie["id"])
            movie["title"] = raw_movie["title"]
            movie["plot_id"] = raw_movie["id"]
            for raw_cast in raw_movie["cast"]:
                try:
                    cast = dict()
                    cast["cast_id"] = raw_cast["cast_id"]
                    cast["character"] = raw_cast["character"]
                    cast["cast_info_id"] = raw_cast["id"]
                    casts.append(cast)
                except:
                    print("Warning: cast info missing!")
            movie["casts"] = casts
            movie["thumbnail_ids"] = [raw_movie["poster_path"]]
            movie["photo_ids"] = []
            movie["video_ids"] = []
            movie["avg_rating"] = raw_movie["vote_average"]
            movie["num_rating"] = raw_movie["vote_count"]
            task = asyncio.ensure_future(
                upload_movie_info(session, addr, movie))
            tasks.append(task)
            plot = dict()
            plot["plot_id"] = raw_movie["id"]
            plot["plot"] = raw_movie["overview"]
            task = asyncio.ensure_future(upload_plot(session, addr, plot))
            tasks.append(task)
            task = asyncio.ensure_future(register_movie(session, addr, movie))
            tasks.append(task)
            idx += 1
            if idx % 200 == 0:
                resps = await asyncio.gather(*tasks)
                print(idx, "movies finished")
        resps = await asyncio.gather(*tasks)
        print(idx, "movies finished")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--cast", action="store", dest="cast_filename",
                        type=str, default="tmdb/casts.json")
    parser.add_argument("-m", "--movie", action="store", dest="movie_filename",
                        type=str, default="tmdb/movies.json")
    parser.add_argument("--server_address", action="store", dest="server_addr",
                        type=str, default="http://nginx-web-server:8080")
    args = parser.parse_args()

    init_register_users(args.server_addr)
    init_register_movies(args.server_addr)

    # with open(args.cast_filename, 'r') as cast_file:
    #     raw_casts = json.load(cast_file)
    # loop = asyncio.get_event_loop()
    # future = asyncio.ensure_future(
    #     write_cast_info(args.server_addr, raw_casts))
    # loop.run_until_complete(future)

    # with open(args.movie_filename, 'r') as movie_file:
    #     raw_movies = json.load(movie_file)
    #     loop = asyncio.get_event_loop()
    #     future = asyncio.ensure_future(
    #         write_movie_info(args.server_addr, raw_movies))
    #     loop.run_until_complete(future)
